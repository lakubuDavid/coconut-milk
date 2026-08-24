#include <cstddef>
#include <cstdio>
#include <exception>
#include <format>
#include <nlohmann/json_fwd.hpp>
#include <optional>
#include <sol/error.hpp>
#include <sol/forward.hpp>
#include <sol/state.hpp>
#include <sol/state_handling.hpp>
#include <sol/table.hpp>
#include <sol/types.hpp>
#include <string>
#include <utility>
#include "app.h"
#include "argparse.h"
#include "bridge.h"
#include "bundle.h"
#include "commands.h"
#include "config.h"
#include "context.h"
#include "core/bridge.h"
#include "core/dispatcher.h"
#include "core/exec_command.h"
#include "core/worker.h"
#include "debug.h"
#include "dispatch.h"
#include "error.h"
#include "generators/generate.h"
#include "main_runtime.h"
#include "messages.h"
#include "modules/registry.h"
#include "modules/store.h"
#include "modules/window.h"
#include "new_project.h"
#include "permissions.h"
#include "routes.h"
#include "view_events.h"
#include "window.h"

// Custom URL scheme handler for coconut:// assets.
#include "platform/scheme_handler.h"

// Platform window creation
#if defined(__APPLE__)
#include "platform/darwin/create_window.h"
#endif

#include <filesystem>
#include <iostream>
#include <memory>
#include <set>
#include <vector>
#include "print.h"

using namespace coconut;

namespace {

  /// Load .g.lua command modules into a core Worker's Lua state, mirroring
  /// the legacy bg_runtime loader. Handlers land in `ctx`'s registry and are
  /// then copied into the worker's own command map (one Lua VM per worker).
  std::optional<coconut::Error> loadWorkerCommands(
      coconut::core::Worker* w, CoconutContext* ctx, const Config& cfg
  ) {
    sol::state& lua     = *w->LuaState;
    std::string cmdRoot = cfg.command_root.empty() ? "commands" : cfg.command_root;
    std::string genDir  = "generated";

    // .g.lua files call ctx:bind(...) — the CoconutContext usertype must be
    // registered BEFORE exposing ctx (same order legacy bg_runtime used).
    coconut::context::registerUsertype(lua);

    std::string pkgPath = ";" + cmdRoot + "/?.lua;" + cmdRoot + "/?/init.lua;" + genDir +
                          "/?.lua;" + genDir + "/?/init.lua";
    lua.script("package.path = package.path .. '" + pkgPath + "'");

    // Expose ctx as a global so register(ctx) inside .g.lua works.
    lua.set("ctx", ctx);

    int loaded = 0;
    // Scan exactly ONE source dir for .g.lua modules — prefer generated/
    // (build-pipeline output) when present; fall back to commands/. This
    // avoids duplicate bindings when both dirs exist. package.path above
    // still includes BOTH so require() resolves modules from either.
    std::vector<std::string> dirs;
    if (std::filesystem::is_directory(genDir)) {
      dirs.push_back(genDir);
    } else if (std::filesystem::is_directory(cmdRoot)) {
      dirs.push_back(cmdRoot);
    }

    // Worker contexts have no command registry of their own (context::create
    // leaves it null; only the main context shares App's). Expose a temporary
    // registry for register(ctx), then copy the bound handlers into the
    // worker's own map. The sol functions reference this worker's Lua VM,
    // so they remain valid after the temporary registry dies.
    coconut::commands::Registry tempRegistry{};
    const bool                  hadRegistry = (ctx->commands != nullptr);
    if (!hadRegistry) {
      ctx->commands = &tempRegistry;
    }
    for (const auto& scanDir : dirs) {
      if (!std::filesystem::is_directory(scanDir))
        continue;
      for (auto& entry : std::filesystem::directory_iterator(scanDir)) {
        auto path = entry.path();
        if (path.extension() != ".lua")
          continue;
        auto stem = path.stem().string();
        // Only <name>.g.lua (skip .g_mt.lua — main-thread modules).
        if (stem.size() < 2 || stem.substr(stem.size() - 2) != ".g")
          continue;
        if (stem.size() >= 5 && stem.substr(stem.size() - 5) == ".g_mt")
          continue;

        auto loadResult = lua.script_file(path.string(), sol::script_pass_on_error);
        if (!loadResult.valid()) {
          sol::error e = loadResult;
          debug::warn(
              std::format("[worker] failed to load {}: {}", path.filename().string(), e.what())
          );
          continue;
        }
        sol::object ret = loadResult;
        if (!ret.is<sol::function>()) {
          debug::warn(std::format("[worker] {} did not return a function", path.filename().string())
          );
          continue;
        }
        auto bindResult = ret.as<sol::function>()(ctx);
        if (!bindResult.valid()) {
          sol::error e = bindResult;
          debug::warn(
              std::format("[worker] register({}) failed: {}", path.stem().string(), e.what())
          );
          continue;
        }
        ++loaded;
      }
    }

    // Copy bound handlers into the worker's own map for execCommand lookup.
    if (!hadRegistry) {
      for (auto& [name, fn] : tempRegistry.handlers) {
        w->Commands[name] = fn;
      }
      ctx->commands = nullptr;  // restore — the temporary registry dies here
    } else if (ctx->commands != nullptr) {
      for (auto& [name, fn] : ctx->commands->handlers) {
        w->Commands[name] = fn;
      }
    }

    if (loaded > 0) {
      debug::info(std::format(
          "[worker] loaded {} background command module(s) ({} commands bound)",
          loaded,
          w->Commands.size()
      ));
    }
    return std::nullopt;
  }

}  // namespace

int main(int argc, char* argv[]) {
  // Step 0: parse command-line args (before anything else).
  auto args = argparse::parse(argc, argv);

  if (args.help) {
    if (args.generate)
      argparse::printGenerateHelp(argv[0]);
    else if (args.bundle)
      argparse::printBundleHelp(argv[0]);
    else if (args.new_cmd)
      argparse::printNewHelp(argv[0]);
    else if (args.run_cmd)
      argparse::printRunHelp(argv[0]);
    else
      argparse::printHelp(argv[0]);
    return 0;
  }

  if (args.version) {
    argparse::printVersion(argv[0]);
    return 0;
  }

  // Subcommand: generate
  if (args.generate) {
    // Change to root if specified
    if (args.root != ".") {
      try {
        std::filesystem::current_path(args.root);
      } catch (const std::exception& e) {
        std::cerr << "error: cannot change directory to '" << args.root << "': " << e.what()
                  << std::endl;
        return 1;
      }
    }

    // Load config to get command_root and output_dir
    std::string cmdRoot    = "commands";
    std::string outDir     = args.out_dir;
    auto        cfg_result = coconut::loadConfig();
    if (cfg_result) {
      cmdRoot = cfg_result->command_root;
      if (args.out_dir == "generated") {
        outDir = cfg_result->output_dir;
      }
    }
    if (args.watch) {
      return generator::runGenerateWatch(cmdRoot, outDir);
    }
    return generator::runGenerate(cmdRoot, outDir);
  }

  // Subcommand: new — scaffold a new project (interactive by default)
  if (args.new_cmd) {
    std::string error;
    if (!coconut::scaffoldProject(args.new_name, args.template_name, args.yes, error)) {
      std::cerr << "error: " << error << std::endl;
      return 1;
    }
    if (!args.new_name.empty()) {
      coconut::println("Project '{}' created. cd {} && coconut", args.new_name, args.new_name);
    }
    return 0;
  }

  // Subcommand: bundle
  if (args.bundle) {
    // Resolve the bundle output directory
    std::string bundleDir = args.out_dir;
    if (bundleDir == "generated")
      bundleDir = "bundle";

    // Change to root if specified
    if (args.root != ".") {
      try {
        std::filesystem::current_path(args.root);
      } catch (const std::exception& e) {
        std::cerr << "error: cannot change directory to '" << args.root << "': " << e.what()
                  << std::endl;
        return 1;
      }
    }

    // Load full config (including dev fields for manifests generation)
    auto cfg_result = coconut::loadConfig();
    if (!cfg_result) {
      const auto err = cfg_result.error();
      std::cerr << "error: config load failed: " << err.message << " (" << err.details << ")"
                << std::endl;
      return 1;
    }

    // Create bundle directory
    std::filesystem::create_directories(bundleDir);

    // Run bundle pipeline
    auto result = coconut::bundle::bundle(cfg_result.value(), bundleDir, args.bytecode_config);
    if (!result) {
      coconut::println(stderr, "{}", result.error().message);
      return 1;
    }
    coconut::println("{}", result.value());
    return 0;
  }

  // Detect if running inside a macOS .app bundle.
  // If so, resolve project root to Contents/Resources/ so config,
  // views, assets, and commands are found relative to the bundle.
#if defined(__APPLE__)
  {
    auto bundle_path = coconut::platform::detectBundleResourcePath();
    if (!bundle_path.empty()) {
      try {
        std::filesystem::current_path(bundle_path);
      } catch (const std::exception& e) {
        std::cerr << "error: cannot use bundle path '" << bundle_path << "': " << e.what()
                  << std::endl;
        return 1;
      }
    }
  }
#endif

  // Change to the specified root directory, if given.
  // If root looks like a file (not a directory), treat it as a positional
  // app arg and stay in CWD.
  if (args.root != ".") {
    if (std::filesystem::exists(args.root) && !std::filesystem::is_directory(args.root)) {
      // Root is a file, not a directory — treat as positional app arg
      args.positional_args.insert(args.positional_args.begin(), args.root);
      args.root = ".";
      debug::info(std::format(
          "root '{}' is a file, treating as positional arg", args.positional_args.front()
      ));
    } else {
      debug::info(std::format("changing root to '{}'", args.root));
      try {
        std::filesystem::current_path(args.root);
      } catch (const std::exception& e) {
        debug::error(std::format("cannot change directory to '{}': {}", args.root, e.what()));
        return 1;
      }
    }
  }

  // C++ defaults (used when config file is absent).
  Config cfg{};

  // Apply --debug flag (config file can override).
  cfg.debug.enabled           = args.debug;
  cfg.debug.showTransportDump = args.debug;

  // Step 1: load config file (keep defaults on failure).
  // Tries coconut.config.lua first, then coconut.config.json, then C++ defaults.
  auto cfg_result = coconut::loadConfig();
  if (cfg_result) {
    cfg = cfg_result.value();
    debug::setLevel(debug::levelFromString(cfg.debug.logLevel));
    debug::info(std::format("config loaded: frameless={}", cfg.frameless));
  } else {
    const auto err = cfg_result.error();
    debug::warn(
        std::format("Config load failed (keeping defaults): {} ({})", err.message, err.details)
    );
    debug::info("Place coconut.config.lua (or coconut.config.json) in the working directory.");
  }

  // Apply CLI config overrides (--frameless, --transparent, --title, etc.).
  // These override whatever the config file says.
  if (args.override_window_width > 0)
    cfg.window_width = args.override_window_width;
  if (args.override_window_height > 0)
    cfg.window_height = args.override_window_height;
  if (args.override_frameless)
    cfg.frameless = true;
  if (args.override_transparent)
    cfg.transparent = true;
  if (args.override_title_given)
    cfg.title = args.override_title;

  // --debug flag overrides config file value.
  if (args.debug) {
    cfg.debug.enabled           = true;
    cfg.debug.showTransportDump = true;
  }

  // Apply darwin.* config to NSBundle at runtime (notification permissions,
  // bundle identifier, etc.) so the OS sees the right values.
#if defined(__APPLE__)
  {
    const auto& dn = cfg.darwin;
    // Resolve effective bundle identifier: darwin.bundle_identifier >
    // darwin.app.id > app.id
    std::string bid = dn.bundle_identifier;
    if (bid.empty())
      bid = dn.app.id;
    if (bid.empty())
      bid = cfg.app.id;

    std::string notifStyle = dn.ns.notification_alert_style;

    coconut::permissions::applyDarwinConfig(bid, notifStyle, dn.ns.usage_descriptions);
    if (!bid.empty()) {
      debug::info(std::format("darwin: applied CFBundleIdentifier='{}'", bid));
    }
  }
#endif

  // Install custom URL scheme handler (coconut://) before webview_create().
  // On macOS this sets the pre-WKWebView-configuration hook.
  // On other platforms this stores the root dir for later registration.
  debug::info("main: installing coconut:// scheme handler...");
  auto app_root = std::filesystem::absolute(".").string();
  platform::installSchemeHandlerHook(app_root);

  debug::info("main: creating app...");
  // Step 2: create app (App core owns webview handle + context).
  // On macOS, if frameless mode is enabled, create a frameless NSWindow
  // BEFORE calling app::create() so the webview uses our frameless window.
  void* nativeWindow = nullptr;
#if defined(__APPLE__)
  if (cfg.frameless) {
    debug::info("main: frameless=true, creating frameless NSWindow...");
    int w        = cfg.window_width > 0 ? cfg.window_width : 1280;
    int h        = cfg.window_height > 0 ? cfg.window_height : 720;
    nativeWindow = coconut::platform::createFramelessWindow(100, 100, w, h);
    if (nativeWindow) {
      debug::info("main: frameless NSWindow created successfully");
    } else {
      debug::warn("main: failed to create frameless window, using default");
    }
  } else {
    debug::info("main: frameless=false, using default window");
  }
  // For now, don't pass the window to webview_create to avoid crashes
  // TODO: fix the crash when passing native window
  nativeWindow = nullptr;
#else
  (void)nativeWindow;
#endif
  auto app_result = coconut::app::create(&cfg, nativeWindow);
  if (!app_result) {
    debug::error(std::format("Failed to create app: {}", app_result.error().message));
    return 1;
  }
  auto* app = app_result.value();

  debug::info("main: creating commands registry...");
  // Step 3: create command registry (needed before ctx:bind is called).
  {
    auto cmd_result = coconut::commands::create(&cfg);
    if (!cmd_result) {
      debug::error(std::format("Failed to create commands registry: {}", cmd_result.error().message)
      );
      coconut::app::destroy(app);
      return 1;
    }
    app->commands          = cmd_result.value();
    app->context->commands = app->commands;
  }

  debug::info("main: creating bridge state...");
  // Step 3b: create bridge state (needed before transport creation).
  {
    auto bridge_result = coconut::bridge::create(&cfg);
    if (!bridge_result) {
      debug::error(std::format("Failed to create bridge state: {}", bridge_result.error().message));
      coconut::app::destroy(app);
      return 1;
    }
    app->bridge_state = bridge_result.value();
  }

  debug::info("main: creating window...");
  // Step 4: create window wrapper using the app-owned webview handle.
  auto window_result = coconut::window::createWindow(&cfg, app->webview);
  if (!window_result) {
    debug::error(std::format("Failed to create window: {}", window_result.error().message));
    coconut::app::destroy(app);
    return 1;
  }
  auto* window         = window_result.value();
  app->window          = window;
  app->context->window = window;

  debug::info("main: creating lua runtime...");
  // Step 5: create Lua runtime.
  auto lua_result = coconut::lua::create(&cfg, app->context);
  if (!lua_result) {
    debug::error(std::format("Failed to create Lua runtime: {}", lua_result.error().message));
    coconut::app::destroy(app);
    return 1;
  }
  auto* lua_runtime = lua_result.value();
  app->lua_state    = lua_runtime;

  // Wire back: runtime needs app for bridge access.
  lua_runtime->app = app;

  // Wire ctx.window Lua binding now that the app pointer is available.
  coconut::lua::wireWindowHandle(lua_runtime);

  // Create the bridge transport and bind JS entry points.
  // Must happen after runtime->app is set (transport needs the App*).
  bridge::createTransport(app);

  // ── Core message architecture (Phase 1) ───────────────────────────
  // Construct WorkerPool + Dispatcher + Bridge alongside the legacy path.
  // Inbound traffic still uses the legacy handlers; this only proves the
  // trio can be built, attached, flushed and torn down inside the real app.
  {
    constexpr int kWorkerCount = 2;

    // Window module target — workers marshal window mutations onto the
    // main run loop via dispatch::post, landing on this webview handle.
    coconut::modules::setWindowTarget(app->webview);

    // Store module forwarding target — same mechanism for worker store ops.
    coconut::modules::setStoreApp(app);

    // One CoconutContext per worker — each Lua VM binds its own handlers,
    // so registries must never be shared across workers.
    for (int i = 0; i < kWorkerCount; ++i) {
      auto ctxResult = coconut::context::create(&cfg);
      if (!ctxResult) {
        debug::warn(
            std::format("core wiring: worker ctx {} failed: {}", i, ctxResult.error().message)
        );
        continue;
      }
      app->worker_contexts.push_back(ctxResult.value());
    }

    size_t nextWorkerCtx = 0;
    auto   poolResult =
        coconut::core::WorkerPool::builder(kWorkerCount)
            .withModules(
                coconut::modules::ModulesFlag::ThreadSafe |
                coconut::modules::ModulesFlag::BG_STUBS | coconut::modules::ModulesFlag::WINDOW |
                coconut::modules::ModulesFlag::CLIPBOARD | coconut::modules::ModulesFlag::NOTIFY |
                coconut::modules::ModulesFlag::OPENURL | coconut::modules::ModulesFlag::DIALOG
            )
            .withOutputNotifier([&app] { coconut::dispatch::notify(app); })
            .withInitializer([&](coconut::core::Worker* w) -> std::optional<coconut::Error> {
              size_t i = nextWorkerCtx++;
              if (i >= app->worker_contexts.size()) {
                return coconut::Error{.message = "no CoconutContext for worker"};
              }
              w->Context = app->worker_contexts[i];
              return loadWorkerCommands(w, w->Context, cfg);
            })
            .build();

    if (!poolResult) {
      debug::warn("core wiring: pool build failed: " + poolResult.error().message);
    } else if (auto attachErr = poolResult.value()->attachAll()) {
      debug::warn("core wiring: pool attach failed: " + attachErr->message);
    } else {
      auto dispatcherResult = coconut::core::DispatcherBuilder{}
                                  .withRuntime(lua_runtime)
                                  .withWorkerPool(std::move(poolResult.value()))
                                  .withTransport(app->bridge_state->transport)
                                  .build();
      if (!dispatcherResult) {
        debug::warn("core wiring: dispatcher build failed: " + dispatcherResult.error().message);
      } else {
        app->dispatcher = std::move(dispatcherResult.value());

        auto bridgeResult =
            coconut::core::Bridge::builder()
                .withTransport(app->bridge_state->transport)
                .withLuaState(sol::state_view(*lua_runtime->lua_state))
                .withSyncExecutor(
                    [app](
                        const std::string& name, const nlohmann::json& args
                    ) -> std::optional<coconut::core::CommandResult> {
                      if (app->commands == nullptr || app->lua_state == nullptr ||
                          app->lua_state->lua_state == nullptr ||
                          app->lua_state->context == nullptr) {
                        return std::nullopt;
                      }
                      // Main-thread registries: builtins/mt stubs
                      // (bind_mt) first, then the main .lua command
                      // registry. Anything else falls through to
                      // the worker path.
                      const auto& mt   = app->commands->mt_handlers;
                      const auto& main = app->commands->handlers;
                      const bool  isMt = mt.find(name) != mt.end();
                      if (!isMt && main.find(name) == main.end()) {
                        return std::nullopt;  // not ours — workers
                      }
                      sol::state_view lua(*app->lua_state->lua_state);
                      return isMt ? std::optional(coconut::core::execCommand(
                                        lua, mt, name, args, app->lua_state->context
                                    ))
                                  : std::optional(coconut::core::execCommand(
                                        lua, main, name, args, app->lua_state->context
                                    ));
                    }
                )
                .withTargetResolver([app]() -> std::string {
                  return app->window != nullptr ? app->window->current_view : "";
                })
                .build();
        if (!bridgeResult) {
          debug::warn("core wiring: bridge build failed: " + bridgeResult.error().message);
        } else {
          auto* dispatcherPtr = app->dispatcher.get();
          bridgeResult.value()->setCommandCallHandler([dispatcherPtr,
                                                       app](coconut::core::CommandCallMessage msg) {
            dispatcherPtr->queue(coconut::core::DispatchMessage{std::move(msg)});
            // Wake the main run loop so the queued call flushes promptly
            // (worker results wake it separately via withOutputNotifier).
            coconut::dispatch::notify(app);
          });
          app->core_bridge = std::move(bridgeResult.value());

          // ── Inbound cutover (Phase 2) ──
          // Register the core Bridge as the transport's message sink. From
          // here on, ALL inbound JS traffic routes through the core path:
          //   kEvent → emitToLua · kCall → sync (mt/main commands)
          //          or workers (async envelope replies).
          // The legacy inline handleCall/handleEvent fallback stays compiled
          // but is unreachable while the callback is registered.
          auto* bridgePtr = app->core_bridge.get();
          app->bridge_state->transport->setMessageCallback(
              [bridgePtr](const coconut::core::JsRPCMessage& msg) { bridgePtr->onInbound(msg); }
          );
          debug::info(std::format(
              "core wiring: trio constructed + inbound routed ({} workers, "
              "async reply protocol)",
              kWorkerCount
          ));
        }
      }
    }
  }

  // Finalize the coconut:// scheme handler after the transport / webview
  // is fully initialized.  On macOS this is a no-op (done via pre-webview
  // hook).  On Windows/Linux this registers the handler now.
  debug::info("main: finalizing coconut:// scheme handler...");
  platform::finalizeSchemeHandler(app->webview);

  // Register Cocoa NSWindow lifecycle observers (resize, focus, blur).
  // These emit bridge events so the frontend can listen with coconut.on().
  lifecycle::registerEvents(app);

  // Expose CLI args as coconut.args (read-only Lua table).
  {
    sol::state_view lua(*lua_runtime->lua_state);
    auto            args_tbl = lua.create_table();
    {
      sol::table pos = lua.create_table();
      for (size_t i = 0; i < args.positional_args.size(); ++i) {
        pos[i + 1] = args.positional_args[i];
      }
      args_tbl["positional"] = pos;
    }
    {
      sol::table named = lua.create_table();
      for (const auto& [k, v] : args.key_value_args) {
        named[k] = v;
      }
      args_tbl["named"] = named;
    }
    {
      sol::table flags = lua.create_table();
      for (size_t i = 0; i < args.flag_args.size(); ++i) {
        flags[args.flag_args[i]] = true;
      }
      args_tbl["flags"] = flags;
    }
    lua["coconut"]["args"] = args_tbl;
    debug::info(std::format(
        "coconut.args set: {} positional, {} named, {} flags",
        args.positional_args.size(),
        args.key_value_args.size(),
        args.flag_args.size()
    ));

    // Also inject into JS for frontend access.
    {
      nlohmann::json j;
      j["positional"] = args.positional_args;
      j["named"]      = args.key_value_args;
      j["flags"]      = args.flag_args;
      std::string js  = std::format(
          "window.__coconut_args = {};"
           "if (window.coconut) coconut.args = window.__coconut_args;",
          j.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace)
      );
      if (app->bridge_state != nullptr && app->bridge_state->transport != nullptr) {
        app->bridge_state->transport->eval(js);
      }
      debug::info("coconut.args injected into JS");
    }
  }

  // ── Framework builtins ─────────────────────────────────────────
  // _js_log: JS error forwarding from coconut.ts (window.onerror /
  // unhandledrejection). Registered here so every app surfaces uncaught
  // JS errors on stderr even without user-defined handlers.
  {
    sol::state_view builtinLua(*lua_runtime->lua_state);
    builtinLua.set_function("_coconut_framework_js_log", [](sol::table params) {
      std::string level   = params["level"].get_or(std::string("error"));
      std::string message = params["message"].get_or(std::string("(no message)"));
      std::string stack   = params["stack"].get_or(std::string(""));
      if (level == "error") {
        debug::error("[js] " + message + (stack.empty() ? "" : "\n    at " + stack));
      } else {
        debug::warn("[js] " + message);
      }
    });
    auto bindResult = builtinLua.safe_script(
        "ctx:bind('_js_log', function(params) _coconut_framework_js_log(params); return true end)"
    );
    if (!bindResult.valid()) {
      sol::error e = bindResult;
      debug::warn(std::format("failed to bind _js_log builtin: {}", e.what()));
    }
  }

  // Step 6: load user entry point (main.lua) and apply coconut.config(ctx).
  // The loadEntryPoint function handles:
  //   • loading main.lua
  //   • calling coconut.config(ctx) if it exists
  //   • merging returned config fields (table) into cfg
  // The ctx setters (setWindowSize, setInitialView) mutate the
  // shared Config in-place — merging app-level overrides on top of the
  // config-file defaults.
  debug::info("main: loading entry point...");
  auto entry_result = coconut::lua::loadEntryPoint(lua_runtime, &cfg);
  if (!entry_result && entry_result.error().code != ErrorCode::Ok) {
    // Log the error but continue — non-fatal; app runs with current config.
    debug::warn(std::format(
        "entry-point: {} ({})", entry_result.error().message, entry_result.error().details
    ));
  }

  // Step 7: load view descriptors into the window.
  // Views come from two sources:
  //   - the config file (coconut.config.lua / coconut.config.json) via
  //     `Config::views`
  //   - the Lua entry point's `coconut.views()` which also populates
  //     `Config::views` during loadEntryPoint
  // Convert each ViewEntry into a window::View and register it.
  // Ownership passes to the window — destroyWindow frees the views.
  for (const auto& [name, entry] : cfg.views) {
    window::ViewKind kind;
    if (entry.kind == "file")
      kind = window::VIEW_KIND_FILE;
    else if (entry.kind == "html")
      kind = window::VIEW_KIND_HTML;
    else if (entry.kind == "url")
      kind = window::VIEW_KIND_URL;
    else {
      debug::info(std::format("skipping view '{}': unknown kind '{}'", name, entry.kind));
      continue;
    }

    debug::info(
        std::format("creating view '{}' ({}, {}...)", name, entry.kind, entry.src.substr(0, 60))
    );
    auto view_result = window::createView(entry.src, kind, std::nullopt);
    if (!view_result) {
      debug::warn(std::format("failed to create view '{}': {}", name, view_result.error().message));
      continue;
    }

    auto* v = new window::View(std::move(*view_result));
    window::addView(window, name, v);
    debug::info(std::format("view '{}' registered", name));
    // Fire "load" lifecycle event through the dispatch queue.
    dispatch::lifecycleEvent(app, name, "load");
  }

  // Route queued lifecycle events through the dispatch system,
  // ensuring Lua state is fully initialized before direct access.
  dispatch::drain(app);

  // Pass view names to the route resolver so coconut://view_name links
  // trigger navigation instead of file serving.
  {
    std::set<std::string> names;
    for (const auto& [name, _] : window->views) {
      names.insert(name);
    }
    routes::setViewNames(names);
  }

  // Set fallback file for SPA routing (empty = no fallback)
  if (!cfg.fallback_file.empty()) {
    routes::setFallbackFile(cfg.fallback_file);
  }

  // Install the WKNavigationDelegate before showing the first view
  // so it's in place when the navigation starts.  This intercepts
  // external links and opens them in the system browser instead of
  // the webview.
  debug::info("main: calling window::installNavDelegate");
  window::installNavDelegate(window);
  debug::info("main: window::installNavDelegate done");

  // Open DevTools before any navigation so the hint script is
  // registered at document start on the first page load.
  if (cfg.debug.enabled) {
    debug::info("main: opening DevTools (debug mode)");
    window::openDevTools(window);
  }

  // Apply native window style (frameless, transparent, etc.) after
  // the Lua entry point has had a chance to set config overrides via
  // coconut.config(ctx).  This must happen before showWindow().
  window::applyWindowStyle(window);

  // Show the initial view once all views are registered.
  if (cfg.initial_view.empty()) {
    debug::warn("no initial_view set — app starts with default blank view");
  } else if (window->views.find(cfg.initial_view) == window->views.end()) {
    debug::warn(std::format("initial_view '{}' not found among registered views", cfg.initial_view)
    );
    debug::info("registered views:");
    for (const auto& [name, _] : window->views) {
      debug::info(std::format("  - {}", name));
    }
  } else {
    // kReady is baked into the webview_init() script (see createTransport).
    // No need to signalReady — it auto-fires when the page loads.
    debug::info(std::format("showing initial view '{}'", cfg.initial_view));
    window::showView(window, cfg.initial_view);
    // Track active view in Lua for event dispatch Tier 1.
    if (lua_runtime && lua_runtime->lua_state) {
      sol::state_view lv(*lua_runtime->lua_state);
      lv["coconut"]["_active_view"] = cfg.initial_view;
    }
    // Fire "mount" lifecycle event through the dispatch queue.
    dispatch::lifecycleEvent(app, cfg.initial_view, "mount");
    dispatch::drain(app);

    // Fire the "ready" lifecycle event — flows through coconut._dispatch.
    // Subscribers can use coconut.on("ready", fn, { once = true }).
    if (lua_runtime && lua_runtime->lua_state) {
      sol::state_view lv(*lua_runtime->lua_state);
      sol::function   dispatch = lv["coconut"]["_dispatch"];
      if (dispatch.valid()) {
        dispatch("ready", sol::table(lv, sol::create), "");
      }
    }
  }

  // Start the dispatch run-loop source so queued events are drained
  // automatically on every iteration of the main loop.
  debug::info("main: starting dispatch system...");
  coconut::dispatch::init(app);

  // Manual HMR (coconut.hotreload()) is always available via Lua.
  // Background-thread auto-watch is deferred to v0.2.0.

  debug::info("main: calling app::run()...");
  coconut::app::run(app);

  // Tear down the dispatch system (drains remaining messages).
  coconut::dispatch::shutdown(app);

  coconut::app::destroy(app);
  return 0;
}
