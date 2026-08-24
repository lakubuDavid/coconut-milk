#include "app.h"
#include "debug.h"
#include "view_events.h"
#include "window.h"

#include <format>
// #include <iostream>

namespace coconut::app {

  std::expected<App *, Error> create(Config *configs, void *nativeWindow) {
    if (configs == nullptr) {
      return std::unexpected(Error{
          .code = ErrorCode::InvalidConfig, .message = "App::create called with null config"});
    }

    auto ctx = context::create(configs);
    if (!ctx) {
      return std::unexpected(ctx.error());
    }

    debug::info("app::create: context created, creating webview...");
    webview_t wv = webview_create(configs->debug.enabled ? 1 : 0, nativeWindow);
    if (wv == nullptr) {
      context::destroy(ctx.value());
      return std::unexpected(Error{
          .code = ErrorCode::WebViewError, .message = "Failed to create webview window"});
    }

    return new App{
        .configs      = configs,
        .context      = ctx.value(),
        .webview      = wv,
        .window       = nullptr,
        .lua_state    = nullptr,
        .bridge_state = nullptr,
        .commands     = nullptr,
        .fs           = nullptr,
        .errors       = {}};
  }

  void destroy(App *app) {
    if (app == nullptr) {
      return;
    }

    // Stop any lifecycle observers from firing during teardown.
    lifecycle::unregisterEvents();

    // Shut down the core worker pool BEFORE its Lua contexts die. Destroying
    // the Dispatcher drops the WorkerPool (WorkerDeleter stops each input
    // queue, letting run loops exit, then joins the threads).
    if (app->dispatcher != nullptr) {
      app->dispatcher.reset();
    }
    if (app->core_bridge != nullptr) {
      app->core_bridge.reset();
    }
    for (auto *wctx : app->worker_contexts) {
      context::destroy(wctx);
    }
    app->worker_contexts.clear();

    if (app->window != nullptr) {
      window::destroyWindow(app->window);
      app->window = nullptr;
    }
    // Destroy command handlers BEFORE Lua — they hold sol functions with
    // references to the Lua state that become dangling after lua::destroy().
    if (app->commands != nullptr) {
      commands::destroy(app->commands);
      app->commands = nullptr;
    }
    if (app->lua_state != nullptr) {
      lua::destroy(app->lua_state);
      app->lua_state = nullptr;
    }
    if (app->bridge_state != nullptr) {
      bridge::destroy(app->bridge_state);
      app->bridge_state = nullptr;
    }
    if (app->fs != nullptr) {
      fs::destroy(app->fs);
      app->fs = nullptr;
    }
    if (app->context != nullptr) {
      context::destroy(app->context);
      app->context = nullptr;
    }

    // Destroy the webview instance last (after window/transport are gone).
    if (app->webview) {
      webview_destroy(app->webview);
      app->webview = nullptr;
    }

    delete app;
  }

  void setConfigs(App *app, Config *cfg) {
    app->configs = cfg;
    if (app->context != nullptr) {
      app->context->configs = cfg;
    }
  }

  void run(App *app) {
    debug::info(std::format("app::run: app={}", static_cast<void *>(app)));
    if (app == nullptr) {
      debug::error("app::run: null app");
      return;
    }

    debug::info(std::format(
        "app::run: window={} lua_state={}",
        static_cast<void *>(app->window),
        static_cast<void *>(app->lua_state)
    ));

    // Block the event loop until the native window closes.
    debug::info("app::run: calling webview_run()");
    webview_run(app->webview);
    debug::info("app::run: webview_run returned (window closed)");
  }

  std::optional<Error> getError(App *app) {
    if (app->errors.empty()) {
      return std::nullopt;
    }
    return app->errors.back();
  }

  void pushError(App *app, Error err) {
    app->errors.push_back(err);
  }

  void pushError(App *app, ErrorCode code, std::string message, std::string details) {
    app->errors.push_back(Error{.code = code, .message = message, .details = details});
  }

}  // namespace coconut::app
