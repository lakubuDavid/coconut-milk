#include "app.h"
#include "window.h"

#include <iostream>

namespace coconut::app {

std::expected<App *, Error> create(Config *configs) {
  if (configs == nullptr) {
    return std::unexpected(
        Error{.code = ErrorCode::InvalidConfig,
              .message = "App::create called with null config"});
  }

  auto ctx = context::create(configs);
  if (!ctx) {
    return std::unexpected(ctx.error());
  }

  // Create the native webview window.
  // TODO: wire Config::debug when the field is added to Config.
  webview_t wv = webview_create(0, NULL);
  if (wv == nullptr) {
    context::destroy(ctx.value());
    return std::unexpected(Error{.code = ErrorCode::WebUiError,
                                 .message = "Failed to create webview window"});
  }

  return new App{.configs = configs,
                 .context = ctx.value(),
                 .webview = wv,
                 .window = nullptr,
                 .lua_state = nullptr,
                 .bridge_state = nullptr,
                 .commands = nullptr,
                 .fs = nullptr,
                 .errors = {}};
}

void destroy(App *app) {
  if (app == nullptr) {
    return;
  }

  if (app->window != nullptr) {
    window::destroyWindow(app->window);
    app->window = nullptr;
  }
  if (app->lua_state != nullptr) {
    lua::destroy(app->lua_state);
    app->lua_state = nullptr;
  }
  if (app->bridge_state != nullptr) {
    bridge::destroy(app->bridge_state);
    app->bridge_state = nullptr;
  }
  if (app->commands != nullptr) {
    commands::destroy(app->commands);
    app->commands = nullptr;
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
  std::cerr << "[debug] app::run: app=" << app << "\n";
  if (app == nullptr) {
    std::cerr << "[debug] app::run: null app\n";
    return;
  }

  std::cerr << "[debug] app::run: window=" << app->window
            << " lua_state=" << app->lua_state << "\n";

  // Block the event loop until the native window closes.
  std::cerr << "[debug] app::run: calling webview_run()\n";
  webview_run(app->webview);
  std::cerr << "[debug] app::run: webview_run returned (window closed)\n";
}

std::optional<Error> getError(App *app) {
  if (app->errors.empty()) {
    return std::nullopt;
  }
  return app->errors.back();
}

void pushError(App *app, Error err) { app->errors.push_back(err); }

void pushError(App *app, ErrorCode code, std::string message,
               std::string details) {
  app->errors.push_back(
      Error{.code = code, .message = message, .details = details});
}

} // namespace coconut::app
