#include "app.h"
#include "argparse.h"
#include "bundle.h"
#include "commands.h"
#include "new_project.h"
#include "config.h"
#include "debug.h"
#include "dispatch.h"
#include "generators/generate.h"
#include "lifecycle.h"
#include "permissions.h"
#include "routes.h"
#include "lua_runtime.h"
#include "window.h"

// Custom URL scheme handler for coconut:// assets.
#include "platform/scheme_handler.h"

// Platform window creation
#if defined(__APPLE__)
#include "platform/darwin/create_window.h"
#endif

#include <print>
#include <filesystem>
#include <iostream>
#include <memory>
#include <print>
#include <set>
#include <vector>

using namespace coconut;

int main(int argc, char* argv[]) {
  // Step 0: parse command-line args (before anything else).
  auto args = argparse::parse(argc, argv);



  if (args.help) {
    if (args.generate)      argparse::printGenerateHelp(argv[0]);
    else if (args.bundle)   argparse::printBundleHelp(argv[0]);
    else if (args.new_cmd)  argparse::printNewHelp(argv[0]);
    else if (args.run_cmd)  argparse::printRunHelp(argv[0]);
    else                    argparse::printHelp(argv[0]);
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
        std::cerr << "error: cannot change directory to '" << args.root << "': " << e.what() << std::endl;
        return 1;
      }
    }

    // Load config to get command_root and output_dir
    std::string cmdRoot = "commands";
    std::string outDir = args.out_dir;
    auto cfg_result = coconut::loadConfig();
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
      std::println("Project '{}' created. cd {} && coconut", args.new_name, args.new_name);
    }
    return 0;
  }

  // Subcommand: bundle
  if (args.bundle) {
    // Resolve the bundle output directory
    std::string bundleDir = args.out_dir;
    if (bundleDir == "generated") bundleDir = "bundle";

    // Change to root if specified
    if (args.root != ".") {
      try {
        std::filesystem::current_path(args.root);
      } catch (const std::exception& e) {
        std::cerr << "error: cannot change directory to '" << args.root << "': " << e.what() << std::endl;
        return 1;
      }
    }

    // Load full config (including dev fields for manifests generation)
    auto cfg_result = coconut::loadConfig();
    if (!cfg_result) {
      const auto err = cfg_result.error();
      std::cerr << "error: config load failed: " << err.message << " (" << err.details << ")" << std::endl;
      return 1;
    }

    // Create bundle directory
    std::filesystem::create_directories(bundleDir);

    // Run bundle pipeline
    auto result = coconut::bundle::bundle(cfg_result.value(), bundleDir, args.bytecode_config);
    if (!result) {
      std::println(stderr, "{}", result.error().message);
      return 1;
    }
    std::println("{}", result.value());
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
        std::cerr << "error: cannot use bundle path '" << bundle_path << "': " << e.what() << std::endl;
        return 1;
      }
    }
  }
#endif

  // Change to the specified root directory, if given.
  // If root looks like a file (not a directory), treat it as a positional
  // app arg and stay in CWD.
  if (args.root != ".") {
    if (std::filesystem::exists(args.root) &&
        !std::filesystem::is_directory(args.root)) {
      // Root is a file, not a directory — treat as positional app arg
      args.positional_args.insert(args.positional_args.begin(), args.root);
      args.root = ".";
      debug::info(std::format("root '{}' is a file, treating as positional arg",
                               args.positional_args.front()));
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
  cfg.debug.enabled = args.debug;
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
    debug::warn(std::format("Config load failed (keeping defaults): {} ({})",
                            err.message, err.details));
    debug::info("Place coconut.config.lua (or coconut.config.json) in the working directory.");
  }

  // Apply CLI config overrides (--frameless, --transparent, --title, etc.).
  // These override whatever the config file says.
  if (args.override_window_width > 0)  cfg.window_width  = args.override_window_width;
  if (args.override_window_height > 0) cfg.window_height = args.override_window_height;
  if (args.override_frameless)          cfg.frameless     = true;
  if (args.override_transparent)        cfg.transparent   = true;
  if (args.override_title_given)        cfg.title         = args.override_title;

  // --debug flag overrides config file value.
  if (args.debug) {
    cfg.debug.enabled = true;
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
    if (bid.empty()) bid = dn.app.id;
    if (bid.empty()) bid = cfg.app.id;

    std::string notifStyle = dn.ns.notification_alert_style;

    coconut::permissions::applyDarwinConfig(
        bid, notifStyle, dn.ns.usage_descriptions);
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
    int w = cfg.window_width > 0 ? cfg.window_width : 1280;
    int h = cfg.window_height > 0 ? cfg.window_height : 720;
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
      debug::error(std::format("Failed to create commands registry: {}",
                                cmd_result.error().message));
      coconut::app::destroy(app);
      return 1;
    }
    app->commands = cmd_result.value();
    app->context->commands = app->commands;
  }

  debug::info("main: creating bridge state...");
  // Step 3b: create bridge state (needed before transport creation).
  {
    auto bridge_result = coconut::bridge::create(&cfg);
    if (!bridge_result) {
      debug::error(std::format("Failed to create bridge state: {}",
                                bridge_result.error().message));
      coconut::app::destroy(app);
      return 1;
    }
    app->bridge_state = bridge_result.value();
  }

  debug::info("main: creating window...");
  // Step 4: create window wrapper using the app-owned webview handle.
  auto window_result = coconut::window::createWindow(&cfg, app->webview);
  if (!window_result) {
    debug::error(std::format("Failed to create window: {}",
                              window_result.error().message));
    coconut::app::destroy(app);
    return 1;
  }
  auto* window = window_result.value();
  app->window = window;
  app->context->window = window;

  debug::info("main: creating lua runtime...");
  // Step 5: create Lua runtime.
  auto lua_result = coconut::lua::create(&cfg, app->context);
  if (!lua_result) {
    debug::error(std::format("Failed to create Lua runtime: {}",
                              lua_result.error().message));
    coconut::app::destroy(app);
    return 1;
  }
  auto* lua_runtime = lua_result.value();
  app->lua_state = lua_runtime;

  // Wire back: runtime needs app for bridge access.
  lua_runtime->app = app;

  // Wire ctx.window Lua binding now that the app pointer is available.
  coconut::lua::wireWindowHandle(lua_runtime);

  // Create the bridge transport and bind JS entry points.
  // Must happen after runtime->app is set (transport needs the App*).
  bridge::createTransport(app);

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
    auto args_tbl = lua.create_table();
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
    debug::info(std::format("coconut.args set: {} positional, {} named, {} flags",
                             args.positional_args.size(),
                             args.key_value_args.size(),
                             args.flag_args.size()));

    // Also inject into JS for frontend access.
    {
      nlohmann::json j;
      j["positional"] = args.positional_args;
      j["named"] = args.key_value_args;
      j["flags"] = args.flag_args;
      std::string js = std::format(
        "window.__coconut_args = {};"
        "if (window.coconut) coconut.args = window.__coconut_args;",
        j.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace));
      webview_eval(app->webview, js.c_str());
      debug::info("coconut.args injected into JS");
    }
  }

  // Step 6: load user entry point (main.lua) and apply coconut.config(ctx).
  // The loadEntryPoint function handles:
  //   • loading main.lua
  //   • calling coconut.config(ctx) if it exists
  //   • merging returned config fields (table) into cfg
  // The ctx setters (setBrowser, setWindowSize, setInitialView) mutate the
  // shared Config in-place — merging app-level overrides on top of the
  // config-file defaults.
  debug::info("main: loading entry point...");
  auto entry_result = coconut::lua::loadEntryPoint(lua_runtime, &cfg);
  if (!entry_result && entry_result.error().code != ErrorCode::Ok) {
    // Log the error but continue — non-fatal; app runs with current config.
    debug::warn(std::format("entry-point: {} ({})",
                             entry_result.error().message,
                             entry_result.error().details));
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
      debug::info(std::format("skipping view '{}': unknown kind '{}'",
                              name, entry.kind));
      continue;
    }

    debug::info(std::format("creating view '{}' ({}, {}...)",
                             name, entry.kind, entry.src.substr(0, 60)));
    auto view_result = window::createView(entry.src, kind, std::nullopt);
    if (!view_result) {
      debug::warn(std::format("failed to create view '{}': {}",
                               name, view_result.error().message));
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
    debug::warn(std::format("initial_view '{}' not found among registered views",
                             cfg.initial_view));
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
      sol::function dispatch = lv["coconut"]["_dispatch"];
      if (dispatch.valid()) {
        dispatch("ready", sol::table(lv, sol::create), "");
      }
    }
  }

  // Start the dispatch run-loop source so queued events are drained
  // automatically on every iteration of the main loop.
  debug::info("main: starting dispatch system...");
  coconut::dispatch::init(app);

  debug::info("main: calling app::run()...");
  coconut::app::run(app);

  // Tear down the dispatch system (drains remaining messages).
  coconut::dispatch::shutdown(app);

  coconut::app::destroy(app);
  return 0;
}
