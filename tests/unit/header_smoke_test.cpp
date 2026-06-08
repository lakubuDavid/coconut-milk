#include "bridge.h"
#include "commands.h"
#include "config.h"
#include "context.h"
#include "error.h"
#include "fs.h"
#include "lua_runtime.h"
#include "window.h"
#include "test.h"

extern "C" {
#include <webui.h>
}

COCONUT_TEST(unit, header_smoke_compiles) {
  coconut::Config config{};

  auto ctx_result = coconut::context::create(&config);
  COCONUT_REQUIRE(ctx_result);
  coconut::CoconutContext* ctx = ctx_result.value();

  auto bridge_result = coconut::bridge::create(&config);
  COCONUT_REQUIRE(bridge_result);
  auto* bridge_state = bridge_result.value();

  auto cmd_result = coconut::commands::create(&config);
  COCONUT_REQUIRE(cmd_result);
  auto* command_registry = cmd_result.value();

  auto lua_result = coconut::lua::create(&config, ctx);
  COCONUT_REQUIRE(lua_result);
  auto* lua_runtime = lua_result.value();

  size_t win_id = webui_new_window();
  COCONUT_REQUIRE(win_id > 0);
  auto win_result = coconut::window::createWindow(&config, win_id);
  COCONUT_REQUIRE(win_result);
  auto* window = win_result.value();

  auto roots_result = coconut::fs::create(&config);
  COCONUT_REQUIRE(roots_result);
  auto* roots = roots_result.value();

  COCONUT_REQUIRE(ctx != nullptr);
  COCONUT_REQUIRE(bridge_state != nullptr);
  COCONUT_REQUIRE(command_registry != nullptr);
  COCONUT_REQUIRE(lua_runtime != nullptr);
  COCONUT_REQUIRE(window != nullptr);
  COCONUT_REQUIRE(roots != nullptr);

  COCONUT_REQUIRE(ctx->configs == &config);
  COCONUT_REQUIRE(bridge_state->configs == &config);
  COCONUT_REQUIRE(command_registry->configs == &config);
  COCONUT_REQUIRE(lua_runtime->configs == &config);
  COCONUT_REQUIRE(lua_runtime->context == ctx);
  COCONUT_REQUIRE(window->configs == &config);
  COCONUT_REQUIRE(roots->configs == &config);

  coconut::fs::destroy(roots);
  coconut::window::destroyWindow(window);
  coconut::lua::destroy(lua_runtime);
  coconut::commands::destroy(command_registry);
  coconut::bridge::destroy(bridge_state);
  coconut::context::destroy(ctx);
}
