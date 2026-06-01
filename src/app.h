#ifndef APP_H
#define APP_H

#include "bridge.h"
#include "commands.h"
#include "config.h"
#include "context.h"
#include "error.h"
#include "fs.h"
#include "lua_runtime.h"
#include "webui.h"

#include <optional>
#include <string>
#include <vector>

namespace coconut {

struct App {
  Config *configs = nullptr;
  CoconutContext *context = nullptr;
  webui::Window *window = nullptr;
  lua::Runtime *lua_state = nullptr;
  bridge::State *bridge_state = nullptr;
  commands::Registry *commands = nullptr;
  fs::Roots *fs = nullptr;
  std::vector<Error> errors;
};

namespace app {
App *create(Config *configs);
void destroy(App *app);

void setConfigs(App *app, Config *cfg);

std::optional<Error> getError(App *app);
void pushError(App *app, Error err);
void pushError(App *app, ErrorCode code, std::string message, std::string details);

void run(App *app);
} // namespace app

} // namespace coconut

#endif // APP_H
