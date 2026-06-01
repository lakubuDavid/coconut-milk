#include "config.h"
#include "context.h"
#include "lua_runtime.h"
#include "test.h"

COCONUT_TEST(unit, lua_runtime_create_and_destroy) {
  coconut::Config config{};
  coconut::CoconutContext *ctx = coconut::context::create(&config);
  coconut::lua::Runtime *runtime = coconut::lua::create(&config, ctx);

  COCONUT_REQUIRE(runtime != nullptr);
  COCONUT_REQUIRE(runtime->configs == &config);
  COCONUT_REQUIRE(runtime->context == ctx);
  COCONUT_REQUIRE(runtime->lua_state != nullptr);

  coconut::lua::destroy(runtime);
  coconut::context::destroy(ctx);
}
