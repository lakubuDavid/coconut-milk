/// Tests Lua runtime bootstrap: global `coconut` namespace, config hook,
/// view registration, and event dispatcher.
///
/// Failures mark still-unimplemented Lua runtime features.

#include "config.h"
#include "context.h"
#include "lua_runtime.h"
#include "test.h"

#include <sol/sol.hpp>

COCONUT_TEST(integration, lua_state_has_coconut_global) {
  coconut::Config cfg{};
  auto ctx = coconut::context::create(&cfg);
  COCONUT_REQUIRE(ctx.has_value());

  auto rt = coconut::lua::create(&cfg, ctx.value());
  COCONUT_REQUIRE(rt.has_value());

  // The `coconut` global must exist after bootstrap.
  sol::state& lua = *rt.value()->lua_state;
  bool has_coconut = lua["coconut"].valid();
  COCONUT_REQUIRE(has_coconut);

  // `coconut` must be a table.
  COCONUT_REQUIRE(lua["coconut"].is<sol::table>());

  coconut::lua::destroy(rt.value());
  coconut::context::destroy(ctx.value());
}

COCONUT_TEST(integration, lua_coconut_config_is_callable) {
  coconut::Config cfg{};
  auto ctx = coconut::context::create(&cfg);
  COCONUT_REQUIRE(ctx.has_value());

  auto rt = coconut::lua::create(&cfg, ctx.value());
  COCONUT_REQUIRE(rt.has_value());

  sol::state& lua = *rt.value()->lua_state;

  // The spec says the app calls coconut.config(ctx).
  // If config doesn't exist or isn't callable, this test fails.
  auto config_fn = lua["coconut"]["config"];
  COCONUT_REQUIRE(config_fn.valid());
  COCONUT_REQUIRE(config_fn.is<sol::function>());

  coconut::lua::destroy(rt.value());
  coconut::context::destroy(ctx.value());
}

COCONUT_TEST(integration, lua_coconut_views_is_callable) {
  coconut::Config cfg{};
  auto ctx = coconut::context::create(&cfg);
  COCONUT_REQUIRE(ctx.has_value());

  auto rt = coconut::lua::create(&cfg, ctx.value());
  COCONUT_REQUIRE(rt.has_value());

  sol::state& lua = *rt.value()->lua_state;

  // The spec says coconut.views() returns a table of named view descriptors.
  auto views_fn = lua["coconut"]["views"];
  COCONUT_REQUIRE(views_fn.valid());
  COCONUT_REQUIRE(views_fn.is<sol::function>());

  coconut::lua::destroy(rt.value());
  coconut::context::destroy(ctx.value());
}

COCONUT_TEST(integration, lua_coconut_events_exists) {
  coconut::Config cfg{};
  auto ctx = coconut::context::create(&cfg);
  COCONUT_REQUIRE(ctx.has_value());

  auto rt = coconut::lua::create(&cfg, ctx.value());
  COCONUT_REQUIRE(rt.has_value());

  sol::state& lua = *rt.value()->lua_state;

  // coconut.events(name, payload, ctx) is the Love2D-like frontend dispatch.
  auto events_fn = lua["coconut"]["events"];
  COCONUT_REQUIRE(events_fn.valid());
  COCONUT_REQUIRE(events_fn.is<sol::function>());

  coconut::lua::destroy(rt.value());
  coconut::context::destroy(ctx.value());
}

COCONUT_TEST(integration, lua_ctx_is_exposed) {
  coconut::Config cfg{};
  auto ctx = coconut::context::create(&cfg);
  COCONUT_REQUIRE(ctx.has_value());
  auto* ctx_ptr = ctx.value();

  auto rt = coconut::lua::create(&cfg, ctx_ptr);
  COCONUT_REQUIRE(rt.has_value());

  sol::state& lua = *rt.value()->lua_state;

  // The `ctx` global must be exposed and match the C++ pointer.
  COCONUT_REQUIRE(lua["ctx"].valid());
  COCONUT_REQUIRE(lua["ctx"].is<coconut::CoconutContext*>());

  coconut::lua::destroy(rt.value());
  coconut::context::destroy(ctx.value());
}
