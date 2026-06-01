#include "bridge.h"
#include "commands.h"
#include "config.h"
#include "context.h"
#include "error.h"
#include "fs.h"
#include "lua_runtime.h"
#include "webui.h"
#include "test.h"

COCONUT_TEST(unit, header_smoke_compiles) {
  coconut::Config config{};
  coconut::CoconutContext *ctx = coconut::context::create(&config);

  auto *bridge_state = coconut::bridge::create(&config);
  auto *command_registry = coconut::commands::create(&config);
  auto *lua_runtime = coconut::lua::create(&config, ctx);
  auto *window = coconut::webui::create(&config);
  auto *roots = coconut::fs::create(&config);

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
  coconut::webui::destroy(window);
  coconut::lua::destroy(lua_runtime);
  coconut::commands::destroy(command_registry);
  coconut::bridge::destroy(bridge_state);
  coconut::context::destroy(ctx);
}
