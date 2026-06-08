/// Tests for the command registry: registration, lookup, and dispatch.
///
/// The registry stores sol::protected_function values, so these tests
/// require a live sol::state to create valid Lua function objects.

#include "commands.h"
#include "config.h"
#include "test.h"

#include <sol/sol.hpp>

static sol::protected_function makeFn(sol::state& lua, int result) {
  // Build and return a Lua function that returns a fixed number.
  lua.script("fn = function(params, ctx) return " + std::to_string(result) + " end");
  return lua["fn"];
}

COCONUT_TEST(integration, registry_accepts_and_stores_handler) {
  coconut::Config cfg{};
  sol::state lua;
  lua.open_libraries(sol::lib::base);

  auto reg = coconut::commands::create(&cfg);
  COCONUT_REQUIRE(reg.has_value());
  auto* registry = reg.value();

  // Registry starts empty.
  COCONUT_REQUIRE(registry->handlers.empty());

  // Register a handler.
  registry->handlers["say_hi"] = makeFn(lua, 42);
  COCONUT_REQUIRE_EQ(registry->handlers.size(), size_t(1));
  COCONUT_REQUIRE(registry->handlers.count("say_hi"));

  coconut::commands::destroy(registry);
}

COCONUT_TEST(integration, registry_stores_multiple_handlers) {
  coconut::Config cfg{};
  sol::state lua;
  lua.open_libraries(sol::lib::base);

  auto reg = coconut::commands::create(&cfg);
  COCONUT_REQUIRE(reg.has_value());
  auto* registry = reg.value();

  registry->handlers["cmd_a"] = makeFn(lua, 10);
  registry->handlers["cmd_b"] = makeFn(lua, 20);
  registry->handlers["cmd_c"] = makeFn(lua, 30);

  COCONUT_REQUIRE_EQ(registry->handlers.size(), size_t(3));

  // Call one to verify it's wired correctly.
  auto result = registry->handlers["cmd_b"](sol::table(lua, sol::create), nullptr);
  COCONUT_REQUIRE(result.valid());
  COCONUT_REQUIRE(result.get_type() == sol::type::number);
  COCONUT_REQUIRE_EQ(result.get<int>(), 20);

  coconut::commands::destroy(registry);
}

COCONUT_TEST(integration, registry_lookup_missing_handler_returns_zero) {
  coconut::Config cfg{};
  sol::state lua;
  lua.open_libraries(sol::lib::base);

  auto reg = coconut::commands::create(&cfg);
  COCONUT_REQUIRE(reg.has_value());
  auto* registry = reg.value();

  // A missing handler should not be found.
  COCONUT_REQUIRE(!registry->handlers.count("i_dont_exist"));

  coconut::commands::destroy(registry);
}

COCONUT_TEST(integration, registry_duplicate_handler_overwrites) {
  coconut::Config cfg{};
  sol::state lua;
  lua.open_libraries(sol::lib::base);

  auto reg = coconut::commands::create(&cfg);
  COCONUT_REQUIRE(reg.has_value());
  auto* registry = reg.value();

  registry->handlers["cmd"] = makeFn(lua, 1);
  registry->handlers["cmd"] = makeFn(lua, 99);

  // Last write wins.
  auto result = registry->handlers["cmd"](sol::table(lua, sol::create), nullptr);
  COCONUT_REQUIRE(result.valid());
  COCONUT_REQUIRE_EQ(result.get<int>(), 99);

  coconut::commands::destroy(registry);
}
