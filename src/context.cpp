#include "context.h"

#include "app.h"
#include "commands.h"
#include "debug.h"
#include "dialog.h"
#include "window.h"

// ObjC runtime for native window operations
#include <objc/message.h>
#include <objc/runtime.h>

// NSInteger/NSUInteger not available from raw ObjC headers alone
typedef long NSInteger;
typedef unsigned long NSUInteger;

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
    if (!app || !app->webview) return;
    using id = struct objc_object*;
    using SEL = struct objc_selector*;
    id win = (id)webview_get_window(app->webview);
    if (win) {
      ((void(*)(id, SEL, id))objc_msgSend)(win, sel_registerName("miniaturize:"), (id)nullptr);
    }
  }

  void CoconutWindowHandle::maximize() {
    if (!app || !app->webview) return;
    using id = struct objc_object*;
    using SEL = struct objc_selector*;
    id win = (id)webview_get_window(app->webview);
    if (win) {
      ((void(*)(id, SEL, id))objc_msgSend)(win, sel_registerName("performZoom:"), (id)nullptr);
    }
  }

  void CoconutWindowHandle::setFullscreen(bool on) {
    if (!app || !app->webview) return;
    using id = struct objc_object*;
    using SEL = struct objc_selector*;
    id win = (id)webview_get_window(app->webview);
    if (win) {
      NSUInteger mask = ((NSUInteger(*)(id, SEL))objc_msgSend)(win, sel_registerName("styleMask"));
      bool isFull = (mask & (1 << 14)) != 0;
      if (isFull != on) {
        ((void(*)(id, SEL, id))objc_msgSend)(win, sel_registerName("toggleFullScreen:"), (id)nullptr);
      }
    }
  }

  void CoconutWindowHandle::toggleFullscreen() {
    if (!app || !app->webview) return;
    using id = struct objc_object*;
    using SEL = struct objc_selector*;
    id win = (id)webview_get_window(app->webview);
    if (win) {
      ((void(*)(id, SEL, id))objc_msgSend)(win, sel_registerName("toggleFullScreen:"), (id)nullptr);
    }
  }

  void CoconutWindowHandle::resize(int w, int h) {
    if (app && app->webview) {
      webview_set_size(app->webview, w, h, WEBVIEW_HINT_NONE);
    }
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
