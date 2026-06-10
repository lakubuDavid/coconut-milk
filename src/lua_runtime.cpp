#include "lua_runtime.h"

#include "app.h"
#include "bridge.h"
#include "debug.h"
#include "dialog.h"
#include "fs.h"

#include <sol/state.hpp>
#include <sol/table.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>

// Some system headers (ObjC runtime transitives) define `nil` as a macro
// which clashes with sol::type::nil and other uses. Undefine it here.
#ifdef nil
#undef nil
#endif

namespace coconut::lua {

std::expected<Runtime *, Error> create(Config *cfg, CoconutContext *ctx) {
  auto runtime =
      new Runtime{.configs = cfg, .context = ctx, .lua_state = nullptr};

  runtime->lua_state = new sol::state();
  runtime->lua_state->open_libraries(
      sol::lib::base, sol::lib::package, sol::lib::io, sol::lib::os,
      sol::lib::table, sol::lib::string, sol::lib::math);

  _bindCoconutLuaApi(runtime);
  _bindViewClass(runtime);
  _bindUserType(runtime);

  // Transport is created by main.cpp after runtime->app is wired.

  return runtime;
}

void _bindCoconutLuaApi(Runtime *runtime) {
  sol::table coconut =
      (*runtime->lua_state)["coconut"].get_or_create<sol::table>();

  // Global frontend emitter: coconut.emit(name, payload)
  coconut.set_function(
      "emit", [runtime](const std::string &name, sol::object payloadObj) {
        if (runtime == nullptr || runtime->app == nullptr) {
          return;
        }

        nlohmann::json payloadJson = nlohmann::json::object();
        if (payloadObj.is<sol::table>()) {
          payloadJson = bridge::toJson(payloadObj.as<sol::table>());
        }

        bridge::emitToJS(runtime->app, name, payloadJson);
      });

  // Debug logging functions exposed to Lua as coconut.log / .info / .warn / .error
  coconut.set_function("log",   [](const std::string& msg) { debug::log(msg); });
  coconut.set_function("info",  [](const std::string& msg) { debug::info(msg); });
  coconut.set_function("warn",  [](const std::string& msg) { debug::warn(msg); });
  coconut.set_function("error", [](const std::string& msg) { debug::error(msg); });

  // Built-in stubs — overridden by user's main.lua when loaded.
  coconut.set_function("config",
      [](CoconutContext* ctx) -> CoconutContext* { return ctx; });
  coconut.set_function("views",
      [](sol::this_state s) -> sol::table {
        return sol::state_view(s).create_table();
      });
  coconut.set_function("events",
      [](const std::string&, sol::object, CoconutContext*) { });

  // ── Dialog bindings: coconut.dialog ─────────────────────────────
  // Exposes native message box and file dialogs to Lua.
  sol::table dialog = (*runtime->lua_state).create_table();

  dialog.set_function("message", [](sol::variadic_args va) -> sol::table {
    sol::state_view lua = va.lua_state();
    std::string title = "Message";
    std::string message;
    std::string kind = "info";
    if (va.size() >= 1 && va[0].is<std::string>()) message = va[0].as<std::string>();
    if (va.size() >= 2 && va[1].is<std::string>()) title = va[1].as<std::string>();
    if (va.size() >= 3 && va[2].is<std::string>()) kind = va[2].as<std::string>();
    auto r = dialog::messageBox(title, message, kind);
    sol::table t = lua.create_table();
    t["confirmed"] = r.confirmed;
    return t;
  });

  dialog.set_function("open", [](sol::variadic_args va) -> sol::table {
    sol::state_view lua = va.lua_state();
    std::string title = "Open File";
    bool multi = false;
    std::vector<dialog::Filter> filters;
    if (va.size() >= 1 && va[0].is<std::string>()) title = va[0].as<std::string>();
    if (va.size() >= 2 && va[1].is<bool>()) multi = va[1].as<bool>();
    auto r = dialog::openFile(title, filters, multi);
    sol::table t = lua.create_table();
    t["confirmed"] = r.confirmed;
    t["path"] = r.path;
    sol::table paths = lua.create_table();
    for (size_t i = 0; i < r.paths.size(); ++i) paths[i + 1] = r.paths[i];
    t["paths"] = paths;
    return t;
  });

  dialog.set_function("save", [](sol::variadic_args va) -> sol::table {
    sol::state_view lua = va.lua_state();
    std::string title = "Save File";
    std::string defaultName;
    if (va.size() >= 1 && va[0].is<std::string>()) title = va[0].as<std::string>();
    if (va.size() >= 2 && va[1].is<std::string>()) defaultName = va[1].as<std::string>();
    auto r = dialog::saveFile(title, defaultName);
    sol::table t = lua.create_table();
    t["confirmed"] = r.confirmed;
    t["path"] = r.path;
    return t;
  });

  coconut["dialog"] = dialog;

  // ── JSON utilities: coconut.json ──────────────────────────────
  // Provides jsonify (Lua table → JSON string) and parse (string → table).
  // Uses nlohmann::json under the hood via the bridge's conversion helpers.
  sol::table json_mod = (*runtime->lua_state).create_table();

  json_mod.set_function("jsonify",
      [](sol::object obj) -> std::string {
        if (!obj.valid() || obj.get_type() == sol::type::lua_nil) return "null";
        if (obj.get_type() != sol::type::table) return "{}";
        auto json = bridge::toJson(obj.as<sol::table>());
        return json.dump();
      });

  json_mod.set_function("parse",
      [runtime](const std::string& str) -> sol::object {
        sol::state_view lua(*runtime->lua_state);
        try {
          auto json = nlohmann::json::parse(str);
          return bridge::toTable(lua, json);
        } catch (const std::exception&) {
          return sol::make_object(lua, sol::lua_nil);
        }
      });

  coconut["json"] = json_mod;

  // ── Filesystem: coconut.fs ─────────────────────────────────────
  // Exposes readText, readBytes, writeText, writeBytes, exists, resolve.
  sol::table fs_mod = (*runtime->lua_state).create_table();

  fs_mod.set_function("readText",
      [](const std::string& path) -> std::string {
        auto result = fs::readText(path);
        if (result) return std::move(*result);
        debug::warn(std::format("fs.readText: {} ({})",
                     result.error().message, path));
        return {};
      });

  // Lua strings are byte-safe, so readBytes returns a Lua string too.
  fs_mod.set_function("readBytes",
      [](const std::string& path) -> std::string {
        auto result = fs::readBytes(path);
        if (result) {
          auto& vec = *result;
          return std::string(
              reinterpret_cast<const char*>(vec.data()), vec.size());
        }
        debug::warn(std::format("fs.readBytes: {} ({})",
                     result.error().message, path));
        return {};
      });

  fs_mod.set_function("writeText",
      [](const std::string& path,
         const std::string& content) -> bool {
        auto result = fs::writeText(path, content);
        if (result) return true;
        debug::warn(std::format("fs.writeText: {} ({})",
                     result.error().message, path));
        return false;
      });

  fs_mod.set_function("writeBytes",
      [](const std::string& path,
         const std::string& data) -> bool {
        std::vector<uint8_t> vec(data.begin(), data.end());
        auto result = fs::writeBytes(path, vec);
        if (result) return true;
        debug::warn(std::format("fs.writeBytes: {} ({})",
                     result.error().message, path));
        return false;
      });

  fs_mod.set_function("exists", [](const std::string& path) -> bool {
    return fs::exists(path);
  });

  fs_mod.set_function("resolve", [](const std::string& root,
                                      const std::string& relpath) -> std::string {
    return fs::resolve(root, relpath);
  });

  coconut["fs"] = fs_mod;

  runtime->lua_state->set("coconut", coconut);
}

void _bindViewClass(Runtime *runtime) {
  // Build the View module as Lua code so the descriptor methods (defineProps,
  // on_load, on_mount, on_unmount, on_frontend_event) are easy to read and
  // maintain.  Each factory creates a table with kind/value and chainable
  // lifecycle methods.
  const char* viewSrc = R"(
    local function makeDescriptor(kind, value)
      return {
        kind = kind,
        value = value,
        _props = {},
        _callbacks = {},

        defineProps = function(self, props)
          self._props = props or {}
          return self
        end,

        on_load = function(self, fn)
          self._callbacks.on_load = fn
          return self
        end,

        on_mount = function(self, fn)
          self._callbacks.on_mount = fn
          return self
        end,

        on_unmount = function(self, fn)
          self._callbacks.on_unmount = fn
          return self
        end,

        on_frontend_event = function(self, name, fn)
          if not self._callbacks.frontend_events then
            self._callbacks.frontend_events = {}
          end
          self._callbacks.frontend_events[name] = fn
          return self
        end,
      }
    end

    View = {
      url  = function(url)  return makeDescriptor('url',  url) end,
      html = function(html) return makeDescriptor('html', html) end,
      load = function(path) return makeDescriptor('file', path) end,
    }
  )";

  runtime->lua_state->script(viewSrc);
  runtime->lua_state->set("View", (*runtime->lua_state)["View"]);
}

void _bindUserType(Runtime *runtime) {
  // setWindowSize receives a Lua table { w = ..., h = ... }.
  // We bind it as a lambda to avoid requiring sol2 to auto-convert the
  // plain CoconutWindowSize struct (which sol2 v3.3 doesn't support
  // for member-pointer-based usertype registration).
  auto setWindowSize = [](CoconutContext* ctx, sol::table t) -> CoconutContext* {
    if (ctx != nullptr && ctx->configs != nullptr) {
      ctx->configs->window_width = t["w"].get_or(1280);
      ctx->configs->window_height = t["h"].get_or(640);
    }
    return ctx;
  };

  // Minimum/maximum window size setters also take { w = ..., h = ... }.
  auto setMinimumWindowSize = [](CoconutContext* ctx, sol::table t) -> CoconutContext* {
    if (ctx != nullptr && ctx->configs != nullptr) {
      ctx->configs->window_min_width = t["w"].get_or(0);
      ctx->configs->window_min_height = t["h"].get_or(0);
    }
    return ctx;
  };

  auto setMaximumWindowSize = [](CoconutContext* ctx, sol::table t) -> CoconutContext* {
    if (ctx != nullptr && ctx->configs != nullptr) {
      ctx->configs->window_max_width = t["w"].get_or(0);
      ctx->configs->window_max_height = t["h"].get_or(0);
    }
    return ctx;
  };

  // Window handle getter — sol::readonly would not work here because
  // the handle pointer may be null during registration.  A sol::property
  // getter/setter pair lets C++ control assignment while Lua can read
  // the current value (even as it changes post-registration).
  runtime->lua_state->new_usertype<CoconutContext>(
      "CoconutContext",
      "window", sol::property(
          [](CoconutContext* ctx) -> CoconutWindowHandle* {
            return ctx ? ctx->window_handle : nullptr;
          },
          [](CoconutContext* ctx, CoconutWindowHandle* h) {
            if (ctx) ctx->window_handle = h;
          }),
      "setBrowser", &CoconutContext::setBrowser,
      "setWindowSize", std::move(setWindowSize),
      "setMinimumWindowSize", std::move(setMinimumWindowSize),
      "setMaximumWindowSize", std::move(setMaximumWindowSize),
      "setMinimumWindowWidth", &CoconutContext::setMinimumWindowWidth,
      "setMinimumWindowHeight", &CoconutContext::setMinimumWindowHeight,
      "setMaximumWindowWidth", &CoconutContext::setMaximumWindowWidth,
      "setMaximumWindowHeight", &CoconutContext::setMaximumWindowHeight,
      "setTitle", &CoconutContext::setTitle,
      "setResizable", &CoconutContext::setResizable,
      "setFrameless", &CoconutContext::setFrameless,
      "setTransparent", &CoconutContext::setTransparent,
      "setInitialView", &CoconutContext::setInitialView,
      "show",   &CoconutContext::show,
      "reload", &CoconutContext::reload,
      "close",  &CoconutContext::close,
      "bind",   &CoconutContext::bind);

  // ── CoconutWindowHandle usertype ───────────────────────────────
  // move takes a table { x = dx, y = dy } — sol3 can't auto-convert
  // CoconutPoint from a Lua table, so we use a lambda.
  auto moveHandle = [](CoconutWindowHandle* h, sol::table t) {
    if (!h) return;
    CoconutPoint pt{};
    pt.x = t["x"].get_or(0);
    pt.y = t["y"].get_or(0);
    h->move(pt);
  };

  runtime->lua_state->new_usertype<CoconutWindowHandle>(
      "CoconutWindow",
      "show",           &CoconutWindowHandle::show,
      "reload",         &CoconutWindowHandle::reload,
      "close",          &CoconutWindowHandle::close,
      "minimize",       &CoconutWindowHandle::minimize,
      "maximize",       &CoconutWindowHandle::maximize,
      "setFullscreen",  &CoconutWindowHandle::setFullscreen,
      "toggleFullscreen", &CoconutWindowHandle::toggleFullscreen,
      "resize",         &CoconutWindowHandle::resize,
      "setMovableByBackground", &CoconutWindowHandle::setMovableByBackground,
      "setBackgroundColor", &CoconutWindowHandle::setBackgroundColor,
      "setPosition",    &CoconutWindowHandle::setPosition,
      "getPosition",    &CoconutWindowHandle::getPosition,
      "move",           std::move(moveHandle));

  // ctx.window is set later (after app pointer is wired) via
  // lua::wireWindowHandle(runtime).

  // Always set the base ctx global so Lua can access CoconutContext methods.
  runtime->lua_state->set("ctx", runtime->context);
}

void destroy(Runtime *runtime) {
  if (runtime == nullptr) {
    return;
  }

  delete runtime->lua_state;
  delete runtime;
}

// ── Command invocation ──────────────────────────────────────────────────

std::expected<sol::object, Error> call(Runtime* runtime,
                                       const std::string& name,
                                       sol::table params) {
  if (runtime == nullptr || runtime->lua_state == nullptr) {
    return std::unexpected(Error{
        .code = ErrorCode::InvalidConfig,
        .message = "call: null runtime or lua_state",
    });
  }
  if (runtime->app == nullptr || runtime->app->commands == nullptr) {
    return std::unexpected(Error{
        .code = ErrorCode::InvalidConfig,
        .message = "call: app or commands registry not wired",
    });
  }

  auto& handlers = runtime->app->commands->handlers;
  auto it = handlers.find(name);
  if (it == handlers.end()) {
    return std::unexpected(Error{
        .code = ErrorCode::CommandNotFound,
        .message = "command not registered: " + name,
    });
  }

  // Handler signature: fn(params, ctx)
  auto result = it->second(params, runtime->context);
  if (!result.valid()) {
    sol::error err = result;
    return std::unexpected(Error{
        .code = ErrorCode::LuaError,
        .message = "command '" + name + "' failed",
        .details = err.what(),
    });
  }

  return result;
}

// ── Entry-point loader ──────────────────────────────────────────────────

std::expected<bool, Error> loadEntryPoint(Runtime* runtime, Config* cfg) {
  if (runtime == nullptr || runtime->lua_state == nullptr) {
    return std::unexpected(Error{
        .code = ErrorCode::InvalidConfig,
        .message = "loadEntryPoint: null runtime or lua_state",
    });
  }

  // Probe for main.lua first — missing file is not an error,
  // it just means the app runs on config-file defaults alone.
  {
    std::ifstream probe("main.lua");
    if (!probe.is_open()) {
      debug::log("no main.lua, skipping entry-point config");
      return false;
    }
  }

  sol::state& lua = *runtime->lua_state;

  // ── Load main.lua ──────────────────────────────────────────────────
  auto entry = lua.safe_script_file("main.lua", sol::script_pass_on_error);
  if (!entry.valid()) {
    sol::error err = entry;
    return std::unexpected(Error{
        .code = ErrorCode::LuaError,
        .message = "failed to load main.lua",
        .details = err.what(),
    });
  }

  debug::info("loaded main.lua");

  // ── coconut.config(ctx) ────────────────────────────────────────────
  sol::object config_fn = lua["coconut"]["config"];
  if (config_fn.is<sol::function>()) {
    debug::info("calling coconut.config(ctx)...");
    // ctx is set as a global by _bindUserType — grab it.
    sol::object ctx_obj = lua["ctx"];

    auto result = config_fn.as<sol::function>()(ctx_obj);
    if (!result.valid()) {
      sol::error err = result;
      return std::unexpected(Error{
          .code = ErrorCode::LuaError,
          .message = "coconut.config(ctx) failed",
          .details = err.what(),
      });
    }

    // If the callback returned a table, merge additional fields.
    // The ctx setters already mutated the Config in-place for
    // setBrowser / setWindowSize / setInitialView calls.
    sol::object ret = result;
    if (ret.is<sol::table>() && cfg != nullptr) {
      sol::table t = ret.as<sol::table>();

      auto mergeStr = [&](const char* key, std::string& field) {
        sol::object v = t[key];
        if (v.is<std::string>()) field = v.as<std::string>();
      };
      auto mergeInt = [&](const char* key, int& field) {
        sol::object v = t[key];
        if (v.is<int>()) field = v.as<int>();
      };

      mergeStr("browser",         cfg->browser);
      mergeInt("window_width",    cfg->window_width);
      mergeInt("window_height",   cfg->window_height);
      mergeInt("window_min_width",  cfg->window_min_width);
      mergeInt("window_min_height", cfg->window_min_height);
      mergeInt("window_max_width",  cfg->window_max_width);
      mergeInt("window_max_height", cfg->window_max_height);
      mergeStr("initial_view",    cfg->initial_view);
      mergeStr("title",            cfg->title);
      mergeStr("view_root",       cfg->view_root);
      mergeStr("asset_root",      cfg->asset_root);
      mergeStr("command_root",    cfg->command_root);

      // Merge views block from returned table.
      sol::object views = t["views"];
      if (views.is<sol::table>()) {
        for (auto& [k, v] : views.as<sol::table>()) {
          if (!v.is<sol::table>()) continue;
          sol::table vt = v.as<sol::table>();
          std::string name = k.as<std::string>();
          std::string kind = vt["kind"].get_or<std::string>("");
          if (kind != "file" && kind != "html" && kind != "url") continue;
          cfg->views[name] = ViewEntry{
              .kind = std::move(kind),
              .src = vt["src"].get_or<std::string>(""),
          };
        }
      }
    }

    debug::info("coconut.config(ctx) applied");
  }

  // ── coconut.views() ────────────────────────────────────────────────
  // App-level view definitions complement those from the config file.
  // Views with the same name overwrite config-file entries.
  debug::info("checking coconut.views()...");
  sol::object views_fn = lua["coconut"]["views"];
  if (views_fn.is<sol::function>()) {
    debug::info("calling coconut.views()...");
    auto views_result = views_fn.as<sol::function>()();
    if (views_result.valid()) {
      sol::object views_obj = views_result;
      if (views_obj.is<sol::table>()) {
        debug::info("coconut.views() returned view descriptors");
        sol::table vt = views_obj.as<sol::table>();
        for (auto& [k, v] : vt) {
          if (!v.is<sol::table>()) continue;
          sol::table desc = v.as<sol::table>();
          std::string name = k.as<std::string>();
          std::string kind = desc["kind"].get_or<std::string>("");
          std::string value = desc["value"].get_or<std::string>("");
          if (kind.empty()) continue;
          cfg->views[name] = ViewEntry{.kind = std::move(kind),
                                        .src = std::move(value)};
          debug::info(std::format("view '{}' ({})", name, cfg->views[name].kind));
        }
      }
    }
  }

  // ── coconut.commands(ctx) [manual override] ──────────────────────
  // If the user's main.lua defines coconut.commands(), call it first.
  // This gives explicit control before the auto-loader runs.
  debug::info("checking coconut.commands()...");
  sol::object cmds_fn = lua["coconut"]["commands"];
  if (cmds_fn.is<sol::function>()) {
    debug::info("calling coconut.commands(ctx)...");
    sol::object ctx_obj = lua["ctx"];
    auto cmds_result = cmds_fn.as<sol::function>()(ctx_obj);
    if (!cmds_result.valid()) {
      sol::error err = cmds_result;
      debug::warn(std::format("coconut.commands(ctx) failed: {}", err.what()));
    } else {
      debug::info("coconut.commands(ctx) applied");
    }
  }

  // ── Auto-load generated commands ──────────────────────────────────
  // Scan the command root directory for .g.lua files.  Each .g.lua
  // exports a register(ctx) function that calls ctx:bind() for each
  // command defined in the corresponding .lua module.
  {
    std::string cmdRoot = cfg ? cfg->command_root : "commands";
    debug::info(std::format("scanning {}/ for .g.lua files...", cmdRoot));

    // Add command root to package.path so the .g.lua's require() works.
    std::string pkgPath = cmdRoot + "/?.lua;" + cmdRoot + "/?/init.lua";
    lua.script("package.path = package.path .. '" + pkgPath + "'");

    sol::object ctx_obj = lua["ctx"];
    if (!ctx_obj.valid()) {
      debug::warn("ctx not available, skipping command auto-load");
    } else {
      int loaded = 0;
      try {
        for (auto& entry : std::filesystem::directory_iterator(cmdRoot)) {
          auto path = entry.path();
          if (path.extension() != ".lua") continue;
          auto stem = path.stem().string();

          // Only load .g.lua files (generated command registration wrappers).
          if (stem.size() < 2 ||
              stem.substr(stem.size() - 2) != ".g")
            continue;

          std::string cmdName =
              stem.substr(0, stem.size() - 2);
          debug::info(std::format("found {}.g.lua, loading...", cmdName));

          // Load the .g.lua file — it returns a register function.
          auto loadResult = lua.script_file(path.string(),
              sol::script_pass_on_error);
          if (!loadResult.valid()) {
            sol::error e = loadResult;
            debug::warn(std::format("failed to load {}: {}",
                                    path.filename().string(), e.what()));
            continue;
          }

          // The returned value should be the register function.
          sol::object ret = loadResult;
          if (!ret.is<sol::function>()) {
            debug::warn(std::format("{} did not return a function (returned type {})",
                                    path.filename().string(),
                                    static_cast<int>(ret.get_type())));
            continue;
          }

          // Call register(ctx).
          auto bindResult =
              ret.as<sol::function>()(ctx_obj);
          if (!bindResult.valid()) {
            sol::error e = bindResult;
            debug::warn(std::format("register({}) failed: {}", cmdName, e.what()));
          } else {
            ++loaded;
            debug::info(std::format("registered {} commands", cmdName));
          }
        }
      } catch (const std::filesystem::filesystem_error& err) {
        debug::info(std::format("no {}/ directory or empty", cmdRoot));
      }
      if (loaded > 0) {
        debug::info(std::format("loaded {} command module(s)", loaded));
      }
    }
  }

  debug::info("loadEntryPoint done");
  return true;
}

void wireWindowHandle(Runtime* runtime) {
  if (runtime == nullptr || runtime->lua_state == nullptr ||
      runtime->context == nullptr || runtime->context->window_handle == nullptr) {
    return;
  }
  // Wire the app pointer so window operations can access the webview.
  runtime->context->window_handle->app = runtime->app;

  // ctx.window is already registered as a sol::property getter on
  // the CoconutContext usertype (see _bindUserType), so Lua reads it
  // from C++ via the getter.  No need to set it from Lua side.

  debug::info("wired ctx.window to CoconutWindowHandle");
}

} // namespace coconut::lua
