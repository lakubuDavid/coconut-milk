#include "app.h"
#include "window.h"

#include <iostream>

extern "C" {
#include <webui.h>
}

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

  // App core owns the WebUI window id.
  const size_t win_id = webui_new_window();
  if (win_id == 0) {
    context::destroy(ctx.value());
    return std::unexpected(Error{.code = ErrorCode::WebUiError,
                                 .message = "Failed to create WebUI window"});
  }

  return new App{.configs = configs,
                 .context = ctx.value(),
                 .window_id = win_id,
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
  }
  if (app->lua_state != nullptr) {
    lua::destroy(app->lua_state);
  }
  if (app->bridge_state != nullptr) {
    bridge::destroy(app->bridge_state);
  }
  if (app->commands != nullptr) {
    commands::destroy(app->commands);
  }
  if (app->fs != nullptr) {
    fs::destroy(app->fs);
  }
  if (app->context != nullptr) {
    context::destroy(app->context);
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

  // The initial view was already shown by main.ccp's showView call.
  // Just block here until the user closes the window.
  std::cerr << "[debug] app::run: calling webui_wait()\n";
  webui_wait();
  std::cerr << "[debug] app::run: webui_wait returned (unexpected)\n";
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
