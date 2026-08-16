/// Tests for the background thread infrastructure:
///   • Registry split (handlers vs mt_handlers)
///   • ctx:bind vs ctx:bind_mt
///   • Background thread lifecycle (create, start, stop, destroy)
///   • Outbox communication (inbox/outbox)
///   • Command routing to correct thread
///   • Edge cases (null app, no bg thread, full queue)

#include "app.h"
#include "bg_runtime.h"
#include "commands.h"
#include "config.h"
#include "context.h"
#include "dispatch.h"
#include "main_runtime.h"
#include "test.h"

#include <sol/sol.hpp>

#include <atomic>
#include <chrono>
#include <thread>

// ── Helpers ──────────────────────────────────────────────────────────────

static sol::protected_function makeFn(sol::state& lua, const std::string& code) {
  lua.script("fn = function(params, ctx) " + code + " end");
  return lua["fn"];
}

static sol::protected_function makeReturnFn(sol::state& lua, int result) {
  return makeFn(lua, "return " + std::to_string(result));
}

/// Wait for a condition with a timeout. Returns true if condition met.
template <typename F>
static bool waitFor(F&& condition, int timeoutMs = 2000) {
  int waited = 0;
  while (!condition() && waited < timeoutMs) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    waited += 10;
  }
  return condition();
}

// ── Registry split (commands.h) ───────────────────────────────────────

COCONUT_TEST(bg_thread, registry_has_mt_handlers) {
  coconut::Config cfg{};
  auto reg = coconut::commands::create(&cfg);
  COCONUT_REQUIRE(reg.has_value());
  auto* registry = reg.value();

  // Both maps exist and are empty.
  COCONUT_REQUIRE(registry->handlers.empty());
  COCONUT_REQUIRE(registry->mt_handlers.empty());

  coconut::commands::destroy(registry);
}

COCONUT_TEST(bg_thread, registry_bg_and_mt_are_independent) {
  coconut::Config cfg{};
  sol::state lua;
  lua.open_libraries(sol::lib::base);

  auto reg = coconut::commands::create(&cfg);
  COCONUT_REQUIRE(reg.has_value());
  auto* registry = reg.value();

  registry->handlers["bg_cmd"] = makeReturnFn(lua, 10);
  registry->mt_handlers["mt_cmd"] = makeReturnFn(lua, 20);

  // bg only in handlers, mt only in mt_handlers.
  COCONUT_REQUIRE_EQ(registry->handlers.size(), size_t(1));
  COCONUT_REQUIRE(registry->handlers.count("bg_cmd"));
  COCONUT_REQUIRE(!registry->handlers.count("mt_cmd"));

  COCONUT_REQUIRE_EQ(registry->mt_handlers.size(), size_t(1));
  COCONUT_REQUIRE(registry->mt_handlers.count("mt_cmd"));
  COCONUT_REQUIRE(!registry->mt_handlers.count("bg_cmd"));

  coconut::commands::destroy(registry);
}

// ── ctx:bind vs ctx:bind_mt ──────────────────────────────────────────

COCONUT_TEST(bg_thread, ctx_bind_stores_in_bg_handlers) {
  coconut::Config cfg{};
  sol::state lua;
  lua.open_libraries(sol::lib::base);

  auto reg = coconut::commands::create(&cfg);
  COCONUT_REQUIRE(reg.has_value());
  auto* registry = reg.value();

  auto ctx = coconut::context::create(&cfg);
  COCONUT_REQUIRE(ctx.has_value());
  auto* context = ctx.value();
  context->commands = registry;

  auto fn = makeReturnFn(lua, 42);
  context->bind("my_cmd", fn);

  // Stored in handlers (background), not mt_handlers.
  COCONUT_REQUIRE_EQ(registry->handlers.size(), size_t(1));
  COCONUT_REQUIRE(registry->handlers.count("my_cmd"));
  COCONUT_REQUIRE(registry->mt_handlers.empty());

  coconut::context::destroy(context);
  coconut::commands::destroy(registry);
}

COCONUT_TEST(bg_thread, ctx_bind_mt_stores_in_mt_handlers) {
  coconut::Config cfg{};
  sol::state lua;
  lua.open_libraries(sol::lib::base);

  auto reg = coconut::commands::create(&cfg);
  COCONUT_REQUIRE(reg.has_value());
  auto* registry = reg.value();

  auto ctx = coconut::context::create(&cfg);
  COCONUT_REQUIRE(ctx.has_value());
  auto* context = ctx.value();
  context->commands = registry;

  auto fn = makeReturnFn(lua, 99);
  context->bind_mt("mt_cmd", fn);

  // Stored in mt_handlers, not handlers.
  COCONUT_REQUIRE_EQ(registry->mt_handlers.size(), size_t(1));
  COCONUT_REQUIRE(registry->mt_handlers.count("mt_cmd"));
  COCONUT_REQUIRE(registry->handlers.empty());

  coconut::context::destroy(context);
  coconut::commands::destroy(registry);
}

COCONUT_TEST(bg_thread, ctx_bind_and_bind_mt_no_cross_contamination) {
  coconut::Config cfg{};
  sol::state lua;
  lua.open_libraries(sol::lib::base);

  auto reg = coconut::commands::create(&cfg);
  COCONUT_REQUIRE(reg.has_value());
  auto* registry = reg.value();

  auto ctx = coconut::context::create(&cfg);
  COCONUT_REQUIRE(ctx.has_value());
  auto* context = ctx.value();
  context->commands = registry;

  auto fn1 = makeReturnFn(lua, 1);
  auto fn2 = makeReturnFn(lua, 2);
  context->bind("bg", fn1);
  context->bind_mt("mt", fn2);

  COCONUT_REQUIRE_EQ(registry->handlers.size(), size_t(1));
  COCONUT_REQUIRE_EQ(registry->mt_handlers.size(), size_t(1));
  COCONUT_REQUIRE(registry->handlers.count("bg"));
  COCONUT_REQUIRE(registry->mt_handlers.count("mt"));

  coconut::context::destroy(context);
  coconut::commands::destroy(registry);
}

COCONUT_TEST(bg_thread, ctx_bind_duplicate_skips) {
  coconut::Config cfg{};
  sol::state lua;
  lua.open_libraries(sol::lib::base);

  auto reg = coconut::commands::create(&cfg);
  COCONUT_REQUIRE(reg.has_value());
  auto* registry = reg.value();

  auto ctx = coconut::context::create(&cfg);
  COCONUT_REQUIRE(ctx.has_value());
  auto* context = ctx.value();
  context->commands = registry;

  auto fn1 = makeReturnFn(lua, 1);
  auto fn2 = makeReturnFn(lua, 2);
  context->bind("dup", fn1);
  context->bind("dup", fn2);  // Should warn and skip.

  COCONUT_REQUIRE_EQ(registry->handlers.size(), size_t(1));

  // Verify first handler is still in place.
  auto result = registry->handlers["dup"](sol::table(lua, sol::create), nullptr);
  COCONUT_REQUIRE(result.valid());
  COCONUT_REQUIRE_EQ(result.get<int>(), 1);

  coconut::context::destroy(context);
  coconut::commands::destroy(registry);
}

COCONUT_TEST(bg_thread, ctx_bind_mt_duplicate_skips) {
  coconut::Config cfg{};
  sol::state lua;
  lua.open_libraries(sol::lib::base);

  auto reg = coconut::commands::create(&cfg);
  COCONUT_REQUIRE(reg.has_value());
  auto* registry = reg.value();

  auto ctx = coconut::context::create(&cfg);
  COCONUT_REQUIRE(ctx.has_value());
  auto* context = ctx.value();
  context->commands = registry;

  auto fn1 = makeReturnFn(lua, 10);
  auto fn2 = makeReturnFn(lua, 20);
  context->bind_mt("dup_mt", fn1);
  context->bind_mt("dup_mt", fn2);  // Should warn and skip.

  COCONUT_REQUIRE_EQ(registry->mt_handlers.size(), size_t(1));

  auto result = registry->mt_handlers["dup_mt"](sol::table(lua, sol::create), nullptr);
  COCONUT_REQUIRE(result.valid());
  COCONUT_REQUIRE_EQ(result.get<int>(), 10);

  coconut::context::destroy(context);
  coconut::commands::destroy(registry);
}

// ── Background thread lifecycle ──────────────────────────────────────

COCONUT_TEST(bg_thread, create_and_destroy_without_start) {
  coconut::Config cfg{};
  // Need a minimal App with configs set.
  coconut::App app{};
  app.configs = &cfg;

  auto bg = coconut::bg_thread::create(&app, &cfg);
  COCONUT_REQUIRE(bg.has_value());

  // Should have its own Lua state, ctx, and commands registry.
  auto* ctx = bg.value();
  COCONUT_REQUIRE(ctx->lua_state != nullptr);
  COCONUT_REQUIRE(ctx->ctx != nullptr);
  COCONUT_REQUIRE(ctx->commands != nullptr);
  COCONUT_REQUIRE(ctx->commands->handlers.empty());
  COCONUT_REQUIRE(ctx->commands->mt_handlers.empty());

  coconut::bg_thread::destroy(ctx);
}

COCONUT_TEST(bg_thread, start_and_stop) {
  coconut::Config cfg{};
  coconut::App app{};
  app.configs = &cfg;

  auto bg = coconut::bg_thread::create(&app, &cfg);
  COCONUT_REQUIRE(bg.has_value());
  app.bg = bg.value();

  // Start the thread.
  coconut::bg_thread::start(app.bg);
  COCONUT_REQUIRE(app.bg->running.load());

  // Give it a moment to enter the run loop.
  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  // Stop and join.
  coconut::bg_thread::stop(app.bg);
  COCONUT_REQUIRE(!app.bg->running.load());

  coconut::bg_thread::destroy(app.bg);
  app.bg = nullptr;
}

COCONUT_TEST(bg_thread, start_stop_multiple_times) {
  coconut::Config cfg{};
  coconut::App app{};
  app.configs = &cfg;

  auto bg = coconut::bg_thread::create(&app, &cfg);
  COCONUT_REQUIRE(bg.has_value());
  app.bg = bg.value();

  // Start/stop cycle 1.
  coconut::bg_thread::start(app.bg);
  std::this_thread::sleep_for(std::chrono::milliseconds(30));
  coconut::bg_thread::stop(app.bg);

  // Start/stop cycle 2.
  coconut::bg_thread::start(app.bg);
  std::this_thread::sleep_for(std::chrono::milliseconds(30));
  coconut::bg_thread::stop(app.bg);

  coconut::bg_thread::destroy(app.bg);
  app.bg = nullptr;
}

// ── Outbox communication with background thread ──────────────────────

COCONUT_TEST(bg_thread, inbox_push_and_pop) {
  coconut::Config cfg{};
  coconut::App app{};
  app.configs = &cfg;

  auto bg = coconut::bg_thread::create(&app, &cfg);
  COCONUT_REQUIRE(bg.has_value());
  app.bg = bg.value();
  coconut::bg_thread::start(app.bg);

  // Push a command message.
  bool pushed = app.bg->inbox.push({
      coconut::dispatch::MessageKind::CommandCall,
      "ping|{}|test-call-1"});
  COCONUT_REQUIRE(pushed);

  // Wait for the bg thread to process it and push a result.
  bool gotResult = waitFor([bg = app.bg]() {
    return !bg->outbox.empty();
  }, 2000);
  COCONUT_REQUIRE(gotResult);

  // Verify the result.
  auto result = app.bg->outbox.pop();
  COCONUT_REQUIRE(result.has_value());
  COCONUT_REQUIRE(result->kind == coconut::dispatch::MessageKind::CommandResult);

  // Should contain callId "test-call-1" and a CommandNotFound error
  // since "ping" isn't registered in the background registry.
  std::string expected = "test-call-1|";
  COCONUT_REQUIRE(result->payload.size() > expected.size());
  COCONUT_REQUIRE(result->payload.substr(0, expected.size()) == expected);

  coconut::bg_thread::stop(app.bg);
  coconut::bg_thread::destroy(app.bg);
  app.bg = nullptr;
}

COCONUT_TEST(bg_thread, execute_registered_command) {
  coconut::Config cfg{};
  coconut::App app{};
  app.configs = &cfg;

  auto bg = coconut::bg_thread::create(&app, &cfg);
  COCONUT_REQUIRE(bg.has_value());
  app.bg = bg.value();

  // Register a command in the background registry BEFORE starting.
  // We do this by loading Lua code into the bg's Lua state.
  {
    sol::state_view bg_lua(app.bg->lua_state->lua_state());
    bg_lua.script(R"lua(
      function test_echo(params, ctx)
        return { received = params.msg or "(nil)" }
      end
    )lua");
    app.bg->commands->handlers["echo"] = bg_lua["test_echo"];
  }

  coconut::bg_thread::start(app.bg);

  // Push a command call with args.
  bool pushed = app.bg->inbox.push({
      coconut::dispatch::MessageKind::CommandCall,
      R"(echo|{"msg":"hello"}|call-echo-1)"});
  COCONUT_REQUIRE(pushed);

  // Wait for the result.
  bool gotResult = waitFor([bg = app.bg]() {
    return !bg->outbox.empty();
  }, 2000);
  COCONUT_REQUIRE(gotResult);

  auto result = app.bg->outbox.pop();
  COCONUT_REQUIRE(result.has_value());
  COCONUT_REQUIRE(result->kind == coconut::dispatch::MessageKind::CommandResult);

  // Payload: "call-echo-1|{\"received\":\"hello\"}"
  COCONUT_REQUIRE(result->payload.substr(0, 12) == "call-echo-1|");
  // Check it contains the expected JSON.
  COCONUT_REQUIRE(result->payload.find("\"received\"") != std::string::npos);
  COCONUT_REQUIRE(result->payload.find("\"hello\"") != std::string::npos);

  coconut::bg_thread::stop(app.bg);
  coconut::bg_thread::destroy(app.bg);
  app.bg = nullptr;
}

COCONUT_TEST(bg_thread, execute_command_with_error) {
  coconut::Config cfg{};
  coconut::App app{};
  app.configs = &cfg;

  auto bg = coconut::bg_thread::create(&app, &cfg);
  COCONUT_REQUIRE(bg.has_value());
  app.bg = bg.value();

  // Register a command that throws an error.
  {
    sol::state_view bg_lua(app.bg->lua_state->lua_state());
    bg_lua.script(R"lua(
      function failing_cmd(params, ctx)
        error("intentional failure")
      end
    )lua");
    app.bg->commands->handlers["fail"] = bg_lua["failing_cmd"];
  }

  coconut::bg_thread::start(app.bg);

  app.bg->inbox.push({
      coconut::dispatch::MessageKind::CommandCall,
      R"(fail|{}|call-fail-1)"});

  bool gotResult = waitFor([bg = app.bg]() {
    return !bg->outbox.empty();
  }, 2000);
  COCONUT_REQUIRE(gotResult);

  auto result = app.bg->outbox.pop();
  COCONUT_REQUIRE(result.has_value());
  COCONUT_REQUIRE(result->kind == coconut::dispatch::MessageKind::CommandResult);

  // Should contain error JSON.
  COCONUT_REQUIRE(result->payload.find("\"code\"") != std::string::npos);
  COCONUT_REQUIRE(result->payload.find("LuaError") != std::string::npos);

  coconut::bg_thread::stop(app.bg);
  coconut::bg_thread::destroy(app.bg);
  app.bg = nullptr;
}

COCONUT_TEST(bg_thread, command_not_found_returns_error) {
  coconut::Config cfg{};
  coconut::App app{};
  app.configs = &cfg;

  auto bg = coconut::bg_thread::create(&app, &cfg);
  COCONUT_REQUIRE(bg.has_value());
  app.bg = bg.value();
  coconut::bg_thread::start(app.bg);

  app.bg->inbox.push({
      coconut::dispatch::MessageKind::CommandCall,
      "nonexistent|{}|call-nf-1"});

  bool gotResult = waitFor([bg = app.bg]() {
    return !bg->outbox.empty();
  }, 2000);
  COCONUT_REQUIRE(gotResult);

  auto result = app.bg->outbox.pop();
  COCONUT_REQUIRE(result.has_value());
  COCONUT_REQUIRE(result->payload.find("CommandNotFound") != std::string::npos);

  coconut::bg_thread::stop(app.bg);
  coconut::bg_thread::destroy(app.bg);
  app.bg = nullptr;
}

// ── Dispatch routing (bridge.cpp logic) ─────────────────────────────

COCONUT_TEST(bg_thread, destroy_without_start) {
  coconut::Config cfg{};
  coconut::App app{};
  app.configs = &cfg;

  auto bg = coconut::bg_thread::create(&app, &cfg);
  COCONUT_REQUIRE(bg.has_value());

  // Destroy without starting — should not crash.
  coconut::bg_thread::destroy(bg.value());
}

COCONUT_TEST(bg_thread, destroy_nullptr) {
  // destroy(nullptr) must not crash.
  coconut::bg_thread::destroy(nullptr);
}

COCONUT_TEST(bg_thread, stop_without_start) {
  coconut::Config cfg{};
  coconut::App app{};
  app.configs = &cfg;

  auto bg = coconut::bg_thread::create(&app, &cfg);
  COCONUT_REQUIRE(bg.has_value());
  app.bg = bg.value();

  // Stop without starting — should not crash, joinable should be false.
  coconut::bg_thread::stop(app.bg);
  COCONUT_REQUIRE(!app.bg->running.load());

  coconut::bg_thread::destroy(app.bg);
  app.bg = nullptr;
}

COCONUT_TEST(bg_thread, start_with_null_bg) {
  // Should not crash.
  coconut::bg_thread::start(nullptr);
}

COCONUT_TEST(bg_thread, stop_with_null_bg) {
  // Should not crash.
  coconut::bg_thread::stop(nullptr);
}

// ── Outbox overflow ─────────────────────────────────────────────────

COCONUT_TEST(bg_thread, inbox_full_returns_false) {
  coconut::Config cfg{};
  coconut::App app{};
  app.configs = &cfg;

  auto bg = coconut::bg_thread::create(&app, &cfg);
  COCONUT_REQUIRE(bg.has_value());
  app.bg = bg.value();

  // Fill the inbox to capacity (64).
  bool allPushed = true;
  for (size_t i = 0; i < coconut::dispatch::kQueueCapacity + 5; ++i) {
    std::string payload = "cmd|{}|id-" + std::to_string(i);
    bool pushed = app.bg->inbox.push({
        coconut::dispatch::MessageKind::CommandCall,
        std::move(payload)});
    if (!pushed) {
      allPushed = false;
      break;
    }
  }

  // Should have failed at some point (queue full).
  COCONUT_REQUIRE(!allPushed);

  // Start the bg thread to drain the queue.
  coconut::bg_thread::start(app.bg);

  // Wait for it to process all messages.
  bool drained = waitFor([bg = app.bg]() {
    return bg->inbox.empty();
  }, 2000);
  COCONUT_REQUIRE(drained);

  coconut::bg_thread::stop(app.bg);
  coconut::bg_thread::destroy(app.bg);
  app.bg = nullptr;
}

// ── Lua binding of bind_mt on CoconutContext usertype ───────────────

COCONUT_TEST(bg_thread, lua_bind_mt_via_usertype) {
  // Verify that bind_mt is callable from Lua via the CoconutContext usertype.
  coconut::Config cfg{};
  sol::state lua;
  lua.open_libraries(sol::lib::base);

  auto reg = coconut::commands::create(&cfg);
  COCONUT_REQUIRE(reg.has_value());

  auto ctx = coconut::context::create(&cfg);
  COCONUT_REQUIRE(ctx.has_value());
  auto* context = ctx.value();
  context->commands = reg.value();

  // Register the usertype so Lua can see bind_mt.
  // We replicate the minimal binding from lua_runtime.cpp.
  lua.new_usertype<coconut::CoconutContext>(
      "CoconutContext",
      "bind",   &coconut::CoconutContext::bind,
      "bind_mt", &coconut::CoconutContext::bind_mt);

  lua.set("ctx", context);

  // Call bind_mt from Lua.
  lua.script(R"lua(
    ctx:bind_mt("lua_mt_cmd", function(params, ctx)
      return { from = "lua", val = params.x or 0 }
    end)
  )lua");

  COCONUT_REQUIRE_EQ(reg.value()->mt_handlers.size(), size_t(1));
  COCONUT_REQUIRE(reg.value()->mt_handlers.count("lua_mt_cmd"));

  // Also verify bind (background) still works.
  lua.script(R"lua(
    ctx:bind("lua_bg_cmd", function(params, ctx)
      return { from = "lua_bg" }
    end)
  )lua");

  COCONUT_REQUIRE_EQ(reg.value()->handlers.size(), size_t(1));
  COCONUT_REQUIRE(reg.value()->handlers.count("lua_bg_cmd"));

  coconut::context::destroy(context);
  coconut::commands::destroy(reg.value());
}

// ── Dispatch outbox drain with bg results ────────────────────────────

COCONUT_TEST(bg_thread, drain_bg_outbox_empty) {
  // drain() with no messages in bg outbox must not crash.
  coconut::App app{};
  coconut::dispatch::drain(&app);
}

COCONUT_TEST(bg_thread, drain_bg_outbox_with_null_bg) {
  // drain() with app->bg == nullptr must not crash.
  coconut::App app{};
  app.bg = nullptr;
  coconut::dispatch::drain(&app);
}

// ── Thread name / identity ───────────────────────────────────────────

COCONUT_TEST(bg_thread, thread_runs_on_different_thread) {
  coconut::Config cfg{};
  coconut::App app{};
  app.configs = &cfg;

  auto mainTid = std::this_thread::get_id();

  auto bg = coconut::bg_thread::create(&app, &cfg);
  COCONUT_REQUIRE(bg.has_value());
  app.bg = bg.value();
  coconut::bg_thread::start(app.bg);

  // Capture the bg thread ID by pushing a command that records it.
  std::thread::id bgTid{};
  {
    sol::state& lua = *app.bg->lua_state;
    lua["record_tid"] = [&bgTid](sol::table params, sol::this_state st) -> sol::object {
      bgTid = std::this_thread::get_id();
      lua_State* L = st;
      return sol::make_object(L, "ok");
    };
    app.bg->commands->handlers["identify"] = lua["record_tid"];
  }

  app.bg->inbox.push({
      coconut::dispatch::MessageKind::CommandCall,
      "identify|{}|tid-call"});

  bool gotResult = waitFor([bg = app.bg]() {
    return !bg->outbox.empty();
  }, 2000);
  COCONUT_REQUIRE(gotResult);

  // The bg thread should have a different thread ID.
  COCONUT_REQUIRE(bgTid != mainTid);
  COCONUT_REQUIRE(bgTid != std::thread::id{});

  coconut::bg_thread::stop(app.bg);
  coconut::bg_thread::destroy(app.bg);
  app.bg = nullptr;
}
