#include "config.h"
#include "context.h"
#include "lua_runtime.h"
#include "test.h"

COCONUT_TEST(unit, lua_runtime_create_and_destroy) {
  coconut::Config config{};

  auto ctx_result = coconut::context::create(&config);
  COCONUT_REQUIRE(ctx_result);
  coconut::CoconutContext* ctx = ctx_result.value();

  auto lua_result = coconut::lua::create(&config, ctx);
  COCONUT_REQUIRE(lua_result);
  coconut::lua::Runtime* runtime = lua_result.value();

  COCONUT_REQUIRE(runtime != nullptr);
  COCONUT_REQUIRE(runtime->configs == &config);
  COCONUT_REQUIRE(runtime->context == ctx);
  COCONUT_REQUIRE(runtime->lua_state != nullptr);

  coconut::lua::destroy(runtime);
  coconut::context::destroy(ctx);
}
