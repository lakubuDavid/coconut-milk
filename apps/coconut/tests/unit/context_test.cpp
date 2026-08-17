#include "app.h"
#include "context.h"
#include "dispatch.h"
#include "test.h"

#include <sol/sol.hpp>

// ── Context creation / destruction ────────────────────────────────────

COCONUT_TEST(unit, context_create_defaults) {
  coconut::Config config{};
  auto result = coconut::context::create(&config);
  COCONUT_REQUIRE(result);
  coconut::CoconutContext* ctx = result.value();
  COCONUT_REQUIRE(ctx != nullptr);
  COCONUT_REQUIRE(ctx->configs == &config);
  COCONUT_REQUIRE(ctx->app == nullptr);
  COCONUT_REQUIRE(ctx->commands == nullptr);
  COCONUT_REQUIRE(ctx->lua_state == nullptr);
  COCONUT_REQUIRE(ctx->window == nullptr);
  coconut::context::destroy(ctx);
}

COCONUT_TEST(unit, context_destroy_null) {
  coconut::context::destroy(nullptr);
  // Must not crash
}

COCONUT_TEST(unit, context_create_null_config) {
  // context::create accepts nullptr config (no validation yet)
  // It creates a valid context with null configs pointer.
  auto result = coconut::context::create(nullptr);
  COCONUT_REQUIRE(result);
  COCONUT_REQUIRE(result.value() != nullptr);
  COCONUT_REQUIRE(result.value()->configs == nullptr);
  coconut::context::destroy(result.value());
}

// ── Context with App ──────────────────────────────────────────────────

COCONUT_TEST(unit, context_with_app) {
  coconut::Config config{};
  auto app_result = coconut::app::create(&config);
  COCONUT_REQUIRE(app_result);
  coconut::App* app = app_result.value();

  auto ctx_result = coconut::context::create(&config);
  COCONUT_REQUIRE(ctx_result);
  coconut::CoconutContext* ctx = ctx_result.value();
  ctx->app = app;

  COCONUT_REQUIRE(ctx->app == app);
  COCONUT_REQUIRE(ctx->configs == &config);

  coconut::context::destroy(ctx);
  coconut::app::destroy(app);
}

// ── Bind / rebind without Lua state (registers in commands registry) ──

COCONUT_TEST(unit, context_bind_handler) {
  coconut::Config config{};
  auto ctx_result = coconut::context::create(&config);
  COCONUT_REQUIRE(ctx_result);
  coconut::CoconutContext* ctx = ctx_result.value();

  // Create a commands registry manually
  auto cmd_result = coconut::commands::create(&config);
  COCONUT_REQUIRE(cmd_result);
  ctx->commands = cmd_result.value();

  // Create a Lua state so we can create a sol::protected_function
  sol::state lua;
  lua.open_libraries(sol::lib::base);

  // Register a handler
  int call_count = 0;
  lua["test_fn"] = [&call_count](sol::table params) {
    (void)params;
    ++call_count;
    return std::string("ok");
  };

  sol::protected_function fn = lua["test_fn"];
  ctx->bind("test_cmd", fn);

  COCONUT_REQUIRE(ctx->commands->handlers.find("test_cmd") != ctx->commands->handlers.end());

  // Invoke it
  auto& handler = ctx->commands->handlers["test_cmd"];
  sol::table empty = lua.create_table();
  auto result = handler(empty);
  COCONUT_REQUIRE(result.valid());
  COCONUT_REQUIRE_EQ(call_count, 1);

  ctx->commands = nullptr;
  coconut::commands::destroy(cmd_result.value());
  coconut::context::destroy(ctx);
}

COCONUT_TEST(unit, context_rebind_handler) {
  coconut::Config config{};
  auto ctx_result = coconut::context::create(&config);
  COCONUT_REQUIRE(ctx_result);
  coconut::CoconutContext* ctx = ctx_result.value();

  auto cmd_result = coconut::commands::create(&config);
  COCONUT_REQUIRE(cmd_result);
  ctx->commands = cmd_result.value();

  sol::state lua;
  lua.open_libraries(sol::lib::base);

  std::string result_str;
  lua["fn1"] = [&result_str](sol::table) { result_str = "first"; return 1; };
  lua["fn2"] = [&result_str](sol::table) { result_str = "second"; return 2; };

  ctx->bind("cmd", lua["fn1"]);
  ctx->rebind("cmd", lua["fn2"]);

  // Should call fn2 (rebind overwrote)
  auto& handler = ctx->commands->handlers["cmd"];
  sol::table empty = lua.create_table();
  auto result = handler(empty);
  COCONUT_REQUIRE(result.valid());
  COCONUT_REQUIRE_EQ(result_str, std::string("second"));

  ctx->commands = nullptr;
  coconut::commands::destroy(cmd_result.value());
  coconut::context::destroy(ctx);
}

COCONUT_TEST(unit, context_bind_mt_registers_separately) {
  coconut::Config config{};
  auto ctx_result = coconut::context::create(&config);
  COCONUT_REQUIRE(ctx_result);
  coconut::CoconutContext* ctx = ctx_result.value();

  auto cmd_result = coconut::commands::create(&config);
  COCONUT_REQUIRE(cmd_result);
  ctx->commands = cmd_result.value();

  sol::state lua;
  lua.open_libraries(sol::lib::base);

  lua["bg_fn"] = [](sol::table) { return "bg"; };
  lua["mt_fn"] = [](sol::table) { return "mt"; };

  ctx->bind("my_cmd", lua["bg_fn"]);
  ctx->bind_mt("my_cmd_mt", lua["mt_fn"]);

  COCONUT_REQUIRE(ctx->commands->handlers.find("my_cmd") != ctx->commands->handlers.end());
  COCONUT_REQUIRE(ctx->commands->mt_handlers.find("my_cmd_mt") != ctx->commands->mt_handlers.end());

  ctx->commands = nullptr;
  coconut::commands::destroy(cmd_result.value());
  coconut::context::destroy(ctx);
}

COCONUT_TEST(unit, context_bind_duplicate_skipped) {
  coconut::Config config{};
  auto ctx_result = coconut::context::create(&config);
  COCONUT_REQUIRE(ctx_result);
  coconut::CoconutContext* ctx = ctx_result.value();

  auto cmd_result = coconut::commands::create(&config);
  COCONUT_REQUIRE(cmd_result);
  ctx->commands = cmd_result.value();

  sol::state lua;
  lua.open_libraries(sol::lib::base);

  int count = 0;
  lua["fn"] = [&count](sol::table) { ++count; return 0; };

  ctx->bind("dup", lua["fn"]);
  ctx->bind("dup", lua["fn"]);  // should be silently skipped

  // Only one handler
  sol::table empty = lua.create_table();
  auto& handler = ctx->commands->handlers["dup"];
  handler(empty);
  COCONUT_REQUIRE_EQ(count, 1);  // only called once

  ctx->commands = nullptr;
  coconut::commands::destroy(cmd_result.value());
  coconut::context::destroy(ctx);
}

// ── Emit / emit_sync (requires App with outbox) ───────────────────────

COCONUT_TEST(unit, context_emit_sync_enqueues_event) {
  coconut::Config config{};
  auto app_result = coconut::app::create(&config);
  COCONUT_REQUIRE(app_result);
  coconut::App* app = app_result.value();

  auto ctx_result = coconut::context::create(&config);
  COCONUT_REQUIRE(ctx_result);
  coconut::CoconutContext* ctx = ctx_result.value();
  ctx->app = app;
  ctx->lua_state = app->lua_state;

  // emit_sync needs a Lua state
  if (app->lua_state != nullptr) {
    sol::state_view lua(*app->lua_state->lua_state);

    sol::table event = lua.create_table();
    event["name"] = "test_event";
    event["value"] = 42;

    // This should not crash (enqueues event in outbox)
    ctx->emit_sync(event);

    // Drain the outbox
    coconut::dispatch::drain(app);
  }

  coconut::context::destroy(ctx);
  coconut::app::destroy(app);
}
