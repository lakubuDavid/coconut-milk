#include "context.h"

#include "app.h"
#include "commands.h"
#include "debug.h"
#include "dialog.h"
#include "window.h"

// Platform dispatch for native window handle operations
#if defined(__APPLE__)
  #include "platform/darwin/window_handle.h"
#elif defined(_WIN32)
  // Win32 stubs for window handle ops (inline nops)
  namespace coconut::window {
    inline void platformMoveWindow(webview_t, int, int) {}
    inline void platformSetWindowPosition(webview_t, int, int) {}
    inline void platformGetWindowPosition(webview_t, int& x, int& y) { x=y=0; }
    inline void platformMinimizeWindow(webview_t) {}
    inline void platformMaximizeWindow(webview_t) {}
    inline void platformToggleFullscreen(webview_t) {}
    inline void platformSetFullscreen(webview_t, bool) {}
    inline void platformSetMovableByBackground(webview_t, bool) {}
    inline void platformSetWindowBackgroundColor(webview_t, float, float, float, float) {}
  }
#elif defined(__linux__)
  // Linux stubs
  namespace coconut::window {
    inline void platformMoveWindow(webview_t, int, int) {}
    inline void platformSetWindowPosition(webview_t, int, int) {}
    inline void platformGetWindowPosition(webview_t, int& x, int& y) { x=y=0; }
    inline void platformMinimizeWindow(webview_t) {}
    inline void platformMaximizeWindow(webview_t) {}
    inline void platformToggleFullscreen(webview_t) {}
    inline void platformSetFullscreen(webview_t, bool) {}
    inline void platformSetMovableByBackground(webview_t, bool) {}
    inline void platformSetWindowBackgroundColor(webview_t, float, float, float, float) {}
  }
#else
  #error "Unsupported platform"
#endif

namespace coconut {

  namespace context {

    std::expected<CoconutContext*, Error> create(Config* config) {
      auto ctx = new CoconutContext{.configs = config,
                                    .app = nullptr,
                                    .bridge_state = nullptr,
                                    .commands = nullptr,
                                    .lua_state = nullptr,
                                    .window = nullptr,
                                    .window_handle = nullptr};
      ctx->window_handle = new CoconutWindowHandle{.app = nullptr};
      return ctx;
    }

    void destroy(CoconutContext* ctx) {
      if (ctx == nullptr) return;
      delete ctx->window_handle;
      delete ctx;
    }

  }  // namespace context

  // ── CoconutContext methods ────────────────────────────────────────

  CoconutContext* CoconutContext::setBrowser(const std::string& mode) {
    if (configs != nullptr) {
      configs->browser = mode;
    }
    return this;
  }

  CoconutContext* CoconutContext::setWindowSize(const CoconutWindowSize& size) {
    if (configs != nullptr) {
      configs->window_width = size.w;
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
      configs->window_min_width = size.w;
      configs->window_min_height = size.h;
    }
    return this;
  }

  CoconutContext* CoconutContext::setMaximumWindowSize(const CoconutWindowSize& size) {
    if (configs != nullptr) {
      configs->window_max_width = size.w;
      configs->window_max_height = size.h;
    }
    return this;
  }

  CoconutContext* CoconutContext::setMinimumWindowWidth(int w) {
    if (configs != nullptr) configs->window_min_width = w;
    return this;
  }

  CoconutContext* CoconutContext::setMinimumWindowHeight(int h) {
    if (configs != nullptr) configs->window_min_height = h;
    return this;
  }

  CoconutContext* CoconutContext::setMaximumWindowWidth(int w) {
    if (configs != nullptr) configs->window_max_width = w;
    return this;
  }

  CoconutContext* CoconutContext::setMaximumWindowHeight(int h) {
    if (configs != nullptr) configs->window_max_height = h;
    return this;
  }

  CoconutContext* CoconutContext::setTitle(const std::string& t) {
    if (configs != nullptr) configs->title = t;
    return this;
  }

  CoconutContext* CoconutContext::setResizable(bool on) {
    if (configs != nullptr) configs->resizable = on;
    return this;
  }

  CoconutContext* CoconutContext::setFrameless(bool on) {
    if (configs != nullptr) configs->frameless = on;
    return this;
  }

  CoconutContext* CoconutContext::setTransparent(bool on) {
    if (configs != nullptr) configs->transparent = on;
    return this;
  }

  void CoconutContext::show(const std::string& name) {
    if (window != nullptr) {
      window::showView(window, name);
    }
  }

  void CoconutContext::reload() {}
  void CoconutContext::close() {}

  void CoconutContext::bind(const std::string& name, sol::protected_function fn) {
    if (commands != nullptr) {
      commands->handlers[name] = fn;
    }
  }

  void CoconutContext::emit(const std::string& name, sol::object payload) {
    (void)name;
    (void)payload;
  }

  void CoconutContext::emit_sync(const std::string& name, sol::object payload) {
    (void)name;
    (void)payload;
  }

  // ── CoconutWindowHandle methods ───────────────────────────────────

  void CoconutWindowHandle::show(const std::string& name) {
    if (app && app->window) window::showView(app->window, name);
  }

  void CoconutWindowHandle::reload() {
    if (app && app->webview) webview_eval(app->webview, "location.reload();");
  }

  void CoconutWindowHandle::close() {
    if (app && app->webview) webview_terminate(app->webview);
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
    static sol::table dialogResultToTable(sol::state_view lua,
                                           const dialog::Result& r) {
      sol::table t = lua.create_table();
      t["confirmed"] = r.confirmed;
      t["path"] = r.path;
      sol::table paths = lua.create_table();
      for (size_t i = 0; i < r.paths.size(); ++i) {
        paths[i + 1] = r.paths[i];
      }
      t["paths"] = paths;
      return t;
    }

  } // anonymous namespace

}  // namespace coconut
