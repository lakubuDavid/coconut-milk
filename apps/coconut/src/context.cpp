#include "context.h"

#include "app.h"
#include "bridge.h"
#include "commands.h"
#include "debug.h"
#include "dialog.h"
#include "dispatch.h"
#include "main_runtime.h"
#include "window.h"

#include <format>

// Platform dispatch for native window handle operations
#include "platform/window_native.h"  // portable platform window ops

namespace coconut {

  namespace context {

    std::expected<CoconutContext*, Error> create(Config* config) {
      CoconutContext* ctx = new CoconutContext{
          .configs       = config,
          .app           = nullptr,
          .bridge_state  = nullptr,
          .commands      = nullptr,
          .lua_state     = nullptr,
          .window        = nullptr,
          .window_handle = nullptr};
      ctx->window_handle = new CoconutWindowHandle{.app = nullptr};
      return ctx;
    }

    void destroy(CoconutContext* ctx) {
      if (ctx == nullptr)
        return;
      delete ctx->window_handle;
      delete ctx;
    }

  }  // namespace context

  // ── CoconutContext methods ────────────────────────────────────────

  CoconutContext* CoconutContext::setWindowSize(const CoconutWindowSize& size) {
    if (configs != nullptr) {
      configs->window_width  = size.w;
      configs->window_height = size.h;
    }
    return this;
  }

  CoconutContext* CoconutContext::setInitialView(const std::string& name) {
    if (configs != nullptr) {
      configs->initial_view = name;
    }
    return this;
  }

  CoconutContext* CoconutContext::setMinimumWindowSize(const CoconutWindowSize& size) {
    if (configs != nullptr) {
      configs->window_min_width  = size.w;
      configs->window_min_height = size.h;
    }
    // Runtime phase: apply live, not just config (window_handle is null
    // during setup — config-only path is unchanged there).
    if (window_handle != nullptr) {
      window_handle->setMinimumSize(size.w, size.h);
    }
    return this;
  }

  CoconutContext* CoconutContext::setMaximumWindowSize(const CoconutWindowSize& size) {
    if (configs != nullptr) {
      configs->window_max_width  = size.w;
      configs->window_max_height = size.h;
    }
    if (window_handle != nullptr) {
      window_handle->setMaximumSize(size.w, size.h);
    }
    return this;
  }

  CoconutContext* CoconutContext::setMinimumWindowWidth(int w) {
    if (configs != nullptr)
      configs->window_min_width = w;
    if (window_handle != nullptr)
      window_handle->setMinimumSize(w, configs ? configs->window_min_height : 0);
    return this;
  }

  CoconutContext* CoconutContext::setMinimumWindowHeight(int h) {
    if (configs != nullptr)
      configs->window_min_height = h;
    if (window_handle != nullptr)
      window_handle->setMinimumSize(configs ? configs->window_min_width : 0, h);
    return this;
  }

  CoconutContext* CoconutContext::setMaximumWindowWidth(int w) {
    if (configs != nullptr)
      configs->window_max_width = w;
    if (window_handle != nullptr)
      window_handle->setMaximumSize(w, configs ? configs->window_max_height : 0);
    return this;
  }

  CoconutContext* CoconutContext::setMaximumWindowHeight(int h) {
    if (configs != nullptr)
      configs->window_max_height = h;
    if (window_handle != nullptr)
      window_handle->setMaximumSize(configs ? configs->window_max_width : 0, h);
    return this;
  }

  CoconutContext* CoconutContext::setTitle(const std::string& t) {
    if (configs != nullptr)
      configs->title = t;
    if (window_handle != nullptr)
      window_handle->setTitle(t);
    return this;
  }

  CoconutContext* CoconutContext::setResizable(bool on) {
    if (configs != nullptr)
      configs->resizable = on;
    if (window_handle != nullptr)
      window_handle->setResizable(on);
    return this;
  }

  CoconutContext* CoconutContext::setFrameless(bool on) {
    if (configs != nullptr)
      configs->frameless = on;
    return this;
  }

  CoconutContext* CoconutContext::setTransparent(bool on) {
    if (configs != nullptr)
      configs->transparent = on;
    return this;
  }

  void CoconutContext::show(const std::string& name) {
    if (window != nullptr) {
      window::showView(window, name);
    }
  }

  void CoconutContext::reload() {
  }
  void CoconutContext::close() {
    // Delegate to window handle if available (which dispatches close event).
    if (window_handle) {
      window_handle->close();
    } else if (window != nullptr) {
      window::showView(window, "");
    }
  }

  void CoconutContext::bind(const std::string& name, sol::protected_function fn) {
    if (commands != nullptr) {
      // Log a warning but skip duplicate command bindings (don't abort).
      if (commands->handlers.find(name) != commands->handlers.end()) {
        debug::warn(
            std::format("duplicate command '{}': a handler is already registered, skipping", name)
        );
        return;
      }
      commands->handlers[name] = fn;
    }
  }

  void CoconutContext::bind_mt(const std::string& name, sol::protected_function fn) {
    if (commands != nullptr) {
      if (commands->mt_handlers.find(name) != commands->mt_handlers.end()) {
        debug::warn(std::format(
            "duplicate command '{}': a main-thread handler is already registered, skipping", name
        ));
        return;
      }
      commands->mt_handlers[name] = fn;
    }
  }

  void CoconutContext::rebind(const std::string& name, sol::protected_function fn) {
    if (commands != nullptr) {
      commands->handlers[name] = fn;
    }
  }

  void CoconutContext::emit(sol::table event) {
    if (!app || !lua_state || !lua_state->lua_state)
      return;

    sol::state_view lua(*lua_state->lua_state);
    std::string     name = event["name"].get_or<std::string>("");
    if (name.empty()) {
      debug::warn("ctx:emit: event must have a 'name' field");
      return;
    }

    // Determine active view for event target
    std::string target = app->window ? app->window->current_view : "";

    // Lua-side dispatch via coconut._dispatch
    sol::function dispatch = lua["coconut"]["_dispatch"];
    if (dispatch.valid()) {
      dispatch(name, event, target);
    }

    // Forward to JS via bridge (strip metatable fields from payload)
    nlohmann::json payloadJson = common::toJson(event);
    payloadJson.erase("name");
    payloadJson.erase("type");
    payloadJson.erase("target");
    payloadJson.erase("defaultPrevented");
    payloadJson.erase("propagationStopped");
    payloadJson.erase("preventDefault");
    payloadJson.erase("stopPropagation");
    payloadJson.erase("stopImmediatePropagation");
    bridge::emitToJS(app, name, payloadJson);
  }

  void CoconutContext::emit_sync(sol::table event) {
    // v1: emit_sync has the same behavior as emit.
    // The sync distinction is a future concern.
    emit(event);
  }

  // ── CoconutWindowHandle methods ───────────────────────────────────

  void CoconutWindowHandle::show(const std::string& name) {
    if (app && app->window && app->window->current_view != name) {
      // Fire "unmount" lifecycle event through the dispatch queue.
      if (!app->window->current_view.empty()) {
        dispatch::lifecycleEvent(app, app->window->current_view, "unmount");
      }
      window::showView(app->window, name);
      // Update Lua's active view tracker for event dispatch Tier 1.
      if (app->lua_state && app->lua_state->lua_state) {
        sol::state_view lua(*app->lua_state->lua_state);
        lua["coconut"]["_active_view"] = name;
      }
      // Fire "mount" lifecycle event through the dispatch queue.
      dispatch::lifecycleEvent(app, name, "mount");
    }
  }

  void CoconutWindowHandle::reload() {
    if (app && app->bridge_state && app->bridge_state->transport)
      app->bridge_state->transport->eval("location.reload();");
  }

  void CoconutWindowHandle::close() {
    if (!app || !app->webview)
      return;

    // Dispatch "close" event through the three-tier chain.
    // If any handler calls event:preventDefault(), the close is vetoed.
    bool shouldClose = true;
    if (app->lua_state && app->lua_state->lua_state) {
      sol::state_view lua(*app->lua_state->lua_state);
      sol::function   dispatch = lua["coconut"]["_dispatch"];
      if (dispatch.valid()) {
        auto result = dispatch("close", sol::table(lua, sol::create), "");
        if (result.valid()) {
          sol::object ev = result;
          if (ev.is<sol::table>()) {
            sol::table eventTable = ev.as<sol::table>();
            if (eventTable["defaultPrevented"].get_or(false)) {
              shouldClose = false;
            }
          }
        }
      }
    }

    if (shouldClose) {
      webview_terminate(app->webview);
    }
  }

  void CoconutWindowHandle::minimize() {
    if (app && app->webview) {
      window::platformMinimizeWindow(app->webview);
    }
  }

  void CoconutWindowHandle::maximize() {
    if (app && app->webview) {
      window::platformMaximizeWindow(app->webview);
    }
  }

  void CoconutWindowHandle::setFullscreen(bool on) {
    if (app && app->webview) {
      window::platformSetFullscreen(app->webview, on);
    }
  }

  void CoconutWindowHandle::toggleFullscreen() {
    if (app && app->webview) {
      window::platformToggleFullscreen(app->webview);
    }
  }

  void CoconutWindowHandle::resize(int w, int h) {
    if (app && app->webview) {
      webview_set_size(app->webview, w, h, WEBVIEW_HINT_NONE);
    }
  }

  void CoconutWindowHandle::setMovableByBackground(bool on) {
    if (app && app->webview) {
      window::platformSetMovableByBackground(app->webview, on);
    }
  }

  void CoconutWindowHandle::setBackgroundColor(float r, float g, float b, float a) {
    if (app && app->webview) {
      window::platformSetWindowBackgroundColor(app->webview, r, g, b, a);
    }
  }

  void CoconutWindowHandle::setTitle(const std::string& title) {
    if (app && app->webview) {
      window::platformSetWindowTitle(app->webview, title);
    }
  }

  void CoconutWindowHandle::setResizable(bool on) {
    if (app && app->webview) {
      window::platformSetResizable(app->webview, on);
    }
  }

  void CoconutWindowHandle::setMinimumSize(int w, int h) {
    if (app && app->webview) {
      window::platformSetMinimumWindowSize(app->webview, w, h);
    }
  }

  void CoconutWindowHandle::setMaximumSize(int w, int h) {
    if (app && app->webview) {
      window::platformSetMaximumWindowSize(app->webview, w, h);
    }
  }

  void CoconutWindowHandle::setPosition(int x, int y) {
    if (app && app->webview) {
      window::platformSetWindowPosition(app->webview, x, y);
    }
  }

  void CoconutWindowHandle::move(const CoconutPoint& offset) {
    if (app && app->webview) {
      window::platformMoveWindow(app->webview, offset.x, offset.y);
    }
  }

  CoconutPoint CoconutWindowHandle::getPosition() {
    CoconutPoint result{};
    if (app && app->webview) {
      window::platformGetWindowPosition(app->webview, result.x, result.y);
    }
    return result;
  }

  // ── Dialog Lua bindings (exposed via coconut.dialog) ──────────────

  namespace {

    /// Convert a C++ dialog::Result to a Lua table.
    static sol::table dialogResultToTable(sol::state_view lua, const dialog::Result& r) {
      sol::table t     = lua.create_table();
      t["confirmed"]   = r.confirmed;
      t["path"]        = r.path;
      sol::table paths = lua.create_table();
      for (size_t i = 0; i < r.paths.size(); ++i) {
        paths[i + 1] = r.paths[i];
      }
      t["paths"] = paths;
      return t;
    }

  }  // anonymous namespace

  // Defined at namespace-coconut scope; qualified name targets
  // coconut::context so it satisfies the header declaration.
  void context::registerUsertype(sol::state_view lua) {
    // Table-taking setters are bound as lambdas — sol2 v3.3 can't auto-
    // convert plain structs (CoconutWindowSize etc.) from Lua tables.
    auto setWindowSize = [](CoconutContext* ctx, sol::table t) -> CoconutContext* {
      if (ctx != nullptr && ctx->configs != nullptr) {
        ctx->configs->window_width  = t["w"].get_or(1280);
        ctx->configs->window_height = t["h"].get_or(640);
      }
      return ctx;
    };
    auto setMinimumWindowSize = [](CoconutContext* ctx, sol::table t) -> CoconutContext* {
      if (ctx != nullptr && ctx->configs != nullptr) {
        ctx->configs->window_min_width  = t["w"].get_or(0);
        ctx->configs->window_min_height = t["h"].get_or(0);
      }
      return ctx;
    };
    auto setMaximumWindowSize = [](CoconutContext* ctx, sol::table t) -> CoconutContext* {
      if (ctx != nullptr && ctx->configs != nullptr) {
        ctx->configs->window_max_width  = t["w"].get_or(0);
        ctx->configs->window_max_height = t["h"].get_or(0);
      }
      return ctx;
    };

    // Window handle getter — sol::readonly would not work here because the
    // handle pointer may be null during registration.
    lua.new_usertype<CoconutContext>(
        "CoconutContext",
        "window",
        sol::property(
            [](CoconutContext* ctx) -> CoconutWindowHandle* {
              return ctx ? ctx->window_handle : nullptr;
            },
            [](CoconutContext* ctx, CoconutWindowHandle* h) {
              if (ctx)
                ctx->window_handle = h;
            }
        ),
        "setWindowSize",
        std::move(setWindowSize),
        "setMinimumWindowSize",
        std::move(setMinimumWindowSize),
        "setMaximumWindowSize",
        std::move(setMaximumWindowSize),
        "setMinimumWindowWidth",
        &CoconutContext::setMinimumWindowWidth,
        "setMinimumWindowHeight",
        &CoconutContext::setMinimumWindowHeight,
        "setMaximumWindowWidth",
        &CoconutContext::setMaximumWindowWidth,
        "setMaximumWindowHeight",
        &CoconutContext::setMaximumWindowHeight,
        "setTitle",
        &CoconutContext::setTitle,
        "setResizable",
        &CoconutContext::setResizable,
        "setFrameless",
        &CoconutContext::setFrameless,
        "setTransparent",
        &CoconutContext::setTransparent,
        "setInitialView",
        &CoconutContext::setInitialView,
        "show",
        &CoconutContext::show,
        "reload",
        &CoconutContext::reload,
        "close",
        &CoconutContext::close,
        "bind",
        &CoconutContext::bind,
        "bind_mt",
        &CoconutContext::bind_mt,
        "rebind",
        &CoconutContext::rebind,
        "emit",
        &CoconutContext::emit,
        "emit_sync",
        &CoconutContext::emit_sync
    );

    // Note: the "ctx" global is intentionally NOT set here — each host
    // (main runtime, worker loader) exposes its own context instance.
  }

}  // namespace coconut
