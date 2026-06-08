#include "lua_runtime.h"

#include "app.h"
#include "bridge.h"

#include <sol/state.hpp>
#include <sol/table.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>

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

  runtime->lua_state->new_usertype<CoconutContext>(
      "CoconutContext", "setBrowser", &CoconutContext::setBrowser,
      "setWindowSize", std::move(setWindowSize), "setInitialView",
      &CoconutContext::setInitialView, "show", &CoconutContext::show, "reload",
      &CoconutContext::reload, "close", &CoconutContext::close, "bind",
      &CoconutContext::bind);

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
      std::cerr << "[debug] no main.lua, skipping entry-point config\n";
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

  std::cerr << "[debug] loaded main.lua\n";

  // ── coconut.config(ctx) ────────────────────────────────────────────
  sol::object config_fn = lua["coconut"]["config"];
  if (config_fn.is<sol::function>()) {
    std::cerr << "[debug]   calling coconut.config(ctx)...\n";
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
      mergeStr("initial_view",    cfg->initial_view);
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

    std::cerr << "[debug] coconut.config(ctx) applied\n";
  }

  // ── coconut.views() ────────────────────────────────────────────────
  // App-level view definitions complement those from the config file.
  // Views with the same name overwrite config-file entries.
  std::cerr << "[debug]   checking coconut.views()...\n";
  sol::object views_fn = lua["coconut"]["views"];
  if (views_fn.is<sol::function>()) {
    std::cerr << "[debug]   calling coconut.views()...\n";
    auto views_result = views_fn.as<sol::function>()();
    if (views_result.valid()) {
      sol::object views_obj = views_result;
      if (views_obj.is<sol::table>()) {
        std::cerr << "[debug]   coconut.views() returned view descriptors\n";
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
          std::cerr << "[debug]     view '" << name << "' ("
                    << cfg->views[name].kind << ")\n";
        }
      }
    }
  }

  // ── Auto-load generated commands ──────────────────────────────────
  // Scan the command root directory for .g.lua files.  Each .g.lua
  // exports a register(ctx) function that calls ctx:bind() for each
  // command defined in the corresponding .lua module.
  {
    std::string cmdRoot = cfg ? cfg->command_root : "commands";
    std::cerr << "[debug]   scanning " << cmdRoot << "/ for .g.lua files...\n";

    // Add command root to package.path so the .g.lua's require() works.
    std::string pkgPath = cmdRoot + "/?.lua;" + cmdRoot + "/?/init.lua";
    lua.script("package.path = package.path .. '" + pkgPath + "'");

    sol::object ctx_obj = lua["ctx"];
    if (!ctx_obj.valid()) {
      std::cerr << "[warn]    ctx not available, skipping command auto-load\n";
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
          std::cerr << "[debug]     found " << cmdName
                    << ".g.lua, loading...\n";

          // Load the .g.lua file — it returns a register function.
          auto loadResult = lua.script_file(path.string(),
              sol::script_pass_on_error);
          if (!loadResult.valid()) {
            sol::error e = loadResult;
            std::cerr << "[warn]      failed to load " << path.filename()
                      << ": " << e.what() << "\n";
            continue;
          }

          // The returned value should be the register function.
          sol::object ret = loadResult;
          if (!ret.is<sol::function>()) {
            std::cerr << "[warn]      " << path.filename()
                      << " did not return a function (returned type "
                      << static_cast<int>(ret.get_type()) << ")\n";
            continue;
          }

          // Call register(ctx).
          auto bindResult =
              ret.as<sol::function>()(ctx_obj);
          if (!bindResult.valid()) {
            sol::error e = bindResult;
            std::cerr << "[warn]      register(" << cmdName
                      << ") failed: " << e.what() << "\n";
          } else {
            ++loaded;
            std::cerr << "[debug]     registered " << cmdName
                      << " commands\n";
          }
        }
      } catch (const std::filesystem::filesystem_error& err) {
        std::cerr << "[debug]   no " << cmdRoot
                  << "/ directory or empty\n";
      }
      if (loaded > 0) {
        std::cerr << "[debug]   loaded " << loaded
                  << " command module(s)\n";
      }
    }
  }

  std::cerr << "[debug] loadEntryPoint done\n";
  return true;
}

} // namespace coconut::lua
