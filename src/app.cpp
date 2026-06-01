#include "app.h"

namespace coconut::app {

App *create(Config *configs) {
  return new App{.configs = configs,
                 .context = context::create(configs),
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
    webui::destroy(app->window);
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
  (void)app;
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

} // namespace coconut::app
