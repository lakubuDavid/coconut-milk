/// Tests for the DOM-like event dispatch system:
///   - coconut._makeEvent() factory
///   - coconut.on() subscribe API (with once, unregister, FIFO)
///   - coconut._dispatch() three-tier chain (view -> subscribe -> fallback)
///   - stopPropagation() / preventDefault() / stopImmediatePropagation()
///   - coconut.emit(event) single-table form
///   - event object shape (name, type getter, target, payload merge)

#include "config.h"
#include "context.h"
#include "lua_runtime.h"
#include "test.h"

#include <sol/sol.hpp>

// ── Helpers ────────────────────────────────────────────────────────────

/// Bootstrap a fresh Lua runtime for each test.
struct EventTestFixture {
  coconut::Config cfg{};
  coconut::CoconutContext* ctx = nullptr;
  coconut::lua::Runtime* runtime = nullptr;
  sol::state* lua = nullptr;

  EventTestFixture() {
    auto ctx_result = coconut::context::create(&cfg);
    // Silence unused-variable warning if REQUIRE is a no-op in release.
    (void)ctx_result;
    COCONUT_REQUIRE(ctx_result.has_value());
    ctx = ctx_result.value();

    auto rt_result = coconut::lua::create(&cfg, ctx);
    COCONUT_REQUIRE(rt_result.has_value());
    runtime = rt_result.value();
    lua = runtime->lua_state;
    COCONUT_REQUIRE(lua != nullptr);
  }

  ~EventTestFixture() {
    coconut::lua::destroy(runtime);
    coconut::context::destroy(ctx);
  }

  /// Run a Lua script that returns a value, and assert it's valid.
  sol::protected_function_result script(const char* src) {
    return lua->safe_script(src, sol::script_pass_on_error);
  }

  /// Run a Lua script that returns a boolean, require true.
  void assertScript(const char* src) {
    auto r = lua->safe_script(src, sol::script_pass_on_error);
    COCONUT_REQUIRE(r.valid());
    if (r.return_count() > 0) {
      sol::object val = r;
      if (val.is<bool>()) {
        COCONUT_REQUIRE(val.as<bool>());
      }
    }
  }
};

// ── _makeEvent factory ─────────────────────────────────────────────────

COCONUT_TEST(unit, event_make_event_creates_object) {
  EventTestFixture f;

  // Return each field individually to avoid metatable getter serialization issues.
  auto r = f.script(R"(
    local e = coconut._makeEvent("test_event", { foo = "bar", num = 42 }, "my_view")
    return { name = e.name, target = e.target, foo = e.foo, num = e.num }
  )");
  COCONUT_REQUIRE(r.valid());
  sol::table t = r;
  COCONUT_REQUIRE_EQ(t["name"].get<std::string>(), "test_event");
  COCONUT_REQUIRE_EQ(t["target"].get<std::string>(), "my_view");
  COCONUT_REQUIRE_EQ(t["foo"].get<std::string>(), "bar");
  COCONUT_REQUIRE_EQ(t["num"].get<int>(), 42);
}

COCONUT_TEST(unit, event_make_event_excludes_name_from_payload) {
  EventTestFixture f;

  auto r = f.script(R"(
    local e = coconut._makeEvent("ev", { name = "override", type = "override2", real = 1 }, "")
    return { name = e.name, real = e.real }
  )");
  COCONUT_REQUIRE(r.valid());
  sol::table t = r;
  COCONUT_REQUIRE_EQ(t["name"].get<std::string>(), "ev");
  COCONUT_REQUIRE_EQ(t["real"].get<int>(), 1);
}

COCONUT_TEST(unit, event_make_event_default_target) {
  EventTestFixture f;

  auto r = f.script(R"(
    local e = coconut._makeEvent("ev", {}, nil)
    return e.target
  )");
  COCONUT_REQUIRE(r.valid());
  COCONUT_REQUIRE_EQ(r.get<std::string>(), "");
}

COCONUT_TEST(unit, event_make_event_methods_exist) {
  EventTestFixture f;

  auto r = f.script(R"(
    local e = coconut._makeEvent("ev", {}, "")
    local ok = (type(e.preventDefault) == "function")
            and (type(e.stopPropagation) == "function")
            and (type(e.stopImmediatePropagation) == "function")
    return ok
  )");
  COCONUT_REQUIRE(r.valid());
  COCONUT_REQUIRE(r.get<bool>());
}

// ── preventDefault / stopPropagation / stopImmediatePropagation ──────

COCONUT_TEST(unit, event_prevent_default_sets_flag) {
  EventTestFixture f;

  auto r = f.script(R"(
    local e = coconut._makeEvent("ev", {}, "")
    assert(not e.defaultPrevented)
    assert(not e.propagationStopped)
    e:preventDefault()
    return e.defaultPrevented
  )");
  COCONUT_REQUIRE(r.valid());
  COCONUT_REQUIRE(r.get<bool>());
}

COCONUT_TEST(unit, event_stop_propagation_sets_flag) {
  EventTestFixture f;

  auto r = f.script(R"(
    local e = coconut._makeEvent("ev", {}, "")
    e:stopPropagation()
    return e.propagationStopped
  )");
  COCONUT_REQUIRE(r.valid());
  COCONUT_REQUIRE(r.get<bool>());
}

COCONUT_TEST(unit, event_stop_immediate_sets_both_flags) {
  EventTestFixture f;

  auto r = f.script(R"(
    local e = coconut._makeEvent("ev", {}, "")
    e:stopImmediatePropagation()
    return { defaultStopped = e.defaultPrevented, propagationStopped = e.propagationStopped }
  )");
  COCONUT_REQUIRE(r.valid());
  sol::table t = r;
  COCONUT_REQUIRE(t["defaultStopped"].get<bool>());
  COCONUT_REQUIRE(t["propagationStopped"].get<bool>());
}

COCONUT_TEST(unit, event_prevent_default_only_sets_default) {
  EventTestFixture f;

  auto r = f.script(R"(
    local e = coconut._makeEvent("ev", {}, "")
    e:preventDefault()
    return e.propagationStopped
  )");
  COCONUT_REQUIRE(r.valid());
  COCONUT_REQUIRE(not r.get<bool>());
}

// ── coconut.on() subscribe API ─────────────────────────────────────────

COCONUT_TEST(unit, event_on_subscribe_fires) {
  EventTestFixture f;

  auto r = f.script(R"(
    local fired = false
    coconut.on("test", function(e) fired = true end)
    local listeners = coconut._listeners["test"]
    return #listeners == 1
  )");
  COCONUT_REQUIRE(r.valid());
  COCONUT_REQUIRE(r.get<bool>());
}

COCONUT_TEST(unit, event_on_unsubscribe_removes) {
  EventTestFixture f;

  auto r = f.script(R"(
    local fired = false
    local unsub = coconut.on("test", function(e) fired = true end)
    unsub()
    local listeners = coconut._listeners["test"]
    return #listeners == 0
  )");
  COCONUT_REQUIRE(r.valid());
  COCONUT_REQUIRE(r.get<bool>());
}

COCONUT_TEST(unit, event_on_subscriber_receives_event) {
  EventTestFixture f;

  auto r = f.script(R"(
    local captured = nil
    coconut.on("test", function(e) captured = e.name end)
    coconut._dispatch("test", { x = 1 }, "")
    return captured
  )");
  COCONUT_REQUIRE(r.valid());
  COCONUT_REQUIRE_EQ(r.get<std::string>(), "test");
}

COCONUT_TEST(unit, event_on_once_fires_only_once) {
  EventTestFixture f;

  auto r = f.script(R"(
    local count = 0
    coconut.on("test", function(e) count = count + 1 end, { once = true })
    coconut._dispatch("test", {}, "")
    coconut._dispatch("test", {}, "")
    return count
  )");
  COCONUT_REQUIRE(r.valid());
  COCONUT_REQUIRE_EQ(r.get<int>(), 1);
}

COCONUT_TEST(unit, event_on_once_removes_after_fire) {
  EventTestFixture f;

  auto r = f.script(R"(
    local count = 0
    coconut.on("test", function(e) count = count + 1 end, { once = true })
    coconut._dispatch("test", {}, "")
    local remaining = #(coconut._listeners["test"] or {})
    return remaining
  )");
  COCONUT_REQUIRE(r.valid());
  COCONUT_REQUIRE_EQ(r.get<int>(), 0);
}

COCONUT_TEST(unit, event_on_fifo_order) {
  EventTestFixture f;

  auto r = f.script(R"(
    local order = {}
    coconut.on("ev", function(e) table.insert(order, "a") end)
    coconut.on("ev", function(e) table.insert(order, "b") end)
    coconut.on("ev", function(e) table.insert(order, "c") end)
    coconut._dispatch("ev", {}, "")
    return order
  )");
  COCONUT_REQUIRE(r.valid());
  sol::table order = r;
  COCONUT_REQUIRE_EQ(order[1].get<std::string>(), "a");
  COCONUT_REQUIRE_EQ(order[2].get<std::string>(), "b");
  COCONUT_REQUIRE_EQ(order[3].get<std::string>(), "c");
}

// ── Three-tier dispatch chain ─────────────────────────────────────────

COCONUT_TEST(unit, event_dispatch_all_tiers_fire) {
  EventTestFixture f;

  auto r = f.script(R"(
    local order = {}

    -- Tier 1: view-scoped
    local view = { _callbacks = { tier_test = function(e) table.insert(order, "view") end } }
    coconut._view_descriptors = coconut._view_descriptors or {}
    coconut._view_descriptors["v"] = view
    coconut._active_view = "v"

    -- Tier 2: global subscribe
    coconut.on("tier_test", function(e) table.insert(order, "sub") end)

    -- Tier 3: fallback
    coconut.events = function(e) table.insert(order, "fallback") end

    coconut._dispatch("tier_test", {}, "v")
    return order
  )");
  COCONUT_REQUIRE(r.valid());
  sol::table order = r;
  COCONUT_REQUIRE_EQ(order[1].get<std::string>(), "view");
  COCONUT_REQUIRE_EQ(order[2].get<std::string>(), "sub");
  COCONUT_REQUIRE_EQ(order[3].get<std::string>(), "fallback");
}

COCONUT_TEST(unit, event_dispatch_tier2_multiple_subscribers) {
  EventTestFixture f;

  auto r = f.script(R"(
    local order = {}
    coconut.on("m", function(e) table.insert(order, "s1") end)
    coconut.on("m", function(e) table.insert(order, "s2") end)
    coconut.events = function(e) table.insert(order, "fb") end
    coconut._dispatch("m", {}, "")
    return order
  )");
  COCONUT_REQUIRE(r.valid());
  sol::table order = r;
  COCONUT_REQUIRE_EQ(order[1].get<std::string>(), "s1");
  COCONUT_REQUIRE_EQ(order[2].get<std::string>(), "s2");
  COCONUT_REQUIRE_EQ(order[3].get<std::string>(), "fb");
}

COCONUT_TEST(unit, event_dispatch_no_view_skips_tier1) {
  EventTestFixture f;

  auto r = f.script(R"(
    local order = {}
    coconut._active_view = nil
    coconut.on("x", function(e) table.insert(order, "sub") end)
    coconut.events = function(e) table.insert(order, "fb") end
    coconut._dispatch("x", {}, "")
    return order
  )");
  COCONUT_REQUIRE(r.valid());
  sol::table order = r;
  COCONUT_REQUIRE_EQ(order[1].get<std::string>(), "sub");
  COCONUT_REQUIRE_EQ(order[2].get<std::string>(), "fb");
}

COCONUT_TEST(unit, event_dispatch_view_not_in_registry_skips_tier1) {
  EventTestFixture f;

  auto r = f.script(R"(
    local order = {}
    coconut._active_view = "nonexistent"
    coconut.on("x", function(e) table.insert(order, "sub") end)
    coconut.events = function(e) table.insert(order, "fb") end
    coconut._dispatch("x", {}, "")
    return order
  )");
  COCONUT_REQUIRE(r.valid());
  sol::table order = r;
  COCONUT_REQUIRE_EQ(order[1].get<std::string>(), "sub");
  COCONUT_REQUIRE_EQ(order[2].get<std::string>(), "fb");
}

// ── Propagation control in dispatch chain ────────────────────────────

COCONUT_TEST(unit, event_stop_propagation_skips_tier3) {
  EventTestFixture f;

  auto r = f.script(R"(
    local order = {}
    coconut.on("sp", function(e) table.insert(order, "sub"); e:stopPropagation() end)
    coconut.events = function(e) table.insert(order, "fb") end
    coconut._dispatch("sp", {}, "")
    return order
  )");
  COCONUT_REQUIRE(r.valid());
  sol::table order = r;
  COCONUT_REQUIRE_EQ(order[1].get<std::string>(), "sub");
  // Tier 3 skipped
  COCONUT_REQUIRE_EQ(order.size(), 1);
}

COCONUT_TEST(unit, event_stop_propagation_skips_subsequent_tiers) {
  EventTestFixture f;

  auto r = f.script(R"(
    local order = {}

    -- View stops propagation
    local view = { _callbacks = { sp2 = function(e) table.insert(order, "view"); e:stopPropagation() end } }
    coconut._view_descriptors = coconut._view_descriptors or {}
    coconut._view_descriptors["v"] = view
    coconut._active_view = "v"

    coconut.on("sp2", function(e) table.insert(order, "sub") end)
    coconut.events = function(e) table.insert(order, "fb") end

    coconut._dispatch("sp2", {}, "v")
    return order
  )");
  COCONUT_REQUIRE(r.valid());
  sol::table order = r;
  COCONUT_REQUIRE_EQ(order[1].get<std::string>(), "view");
  // Tier 2 and 3 skipped
  COCONUT_REQUIRE_EQ(order.size(), 1);
}

COCONUT_TEST(unit, event_stop_propagation_does_not_affect_default) {
  EventTestFixture f;

  auto r = f.script(R"(
    local e = coconut._makeEvent("ev", {}, "")
    e:stopPropagation()
    return e.defaultPrevented
  )");
  COCONUT_REQUIRE(r.valid());
  COCONUT_REQUIRE(not r.get<bool>());
}

COCONUT_TEST(unit, event_dispatch_returns_event_with_default_flag) {
  EventTestFixture f;

  auto r = f.script(R"(
    coconut.on("q", function(e) e:preventDefault() end)
    local e = coconut._dispatch("q", {}, "")
    return e.defaultPrevented
  )");
  COCONUT_REQUIRE(r.valid());
  COCONUT_REQUIRE(r.get<bool>());
}

// ── coconut.emit(event) single-table form ────────────────────────────

COCONUT_TEST(unit, event_emit_runs_dispatch_chain) {
  EventTestFixture f;

  auto r = f.script(R"(
    local order = {}
    coconut.on("emit_test", function(e) table.insert(order, "sub") end)
    coconut.events = function(e) table.insert(order, "fb") end
    coconut.emit({ name = "emit_test" })
    return order
  )");
  COCONUT_REQUIRE(r.valid());
  sol::table order = r;
  COCONUT_REQUIRE_EQ(order[1].get<std::string>(), "sub");
  COCONUT_REQUIRE_EQ(order[2].get<std::string>(), "fb");
}

COCONUT_TEST(unit, event_emit_merges_payload_fields) {
  EventTestFixture f;

  auto r = f.script(R"(
    local captured = nil
    coconut.on("emit_payload", function(e) captured = { name = e.name, msg = e.msg, n = e.n } end)
    coconut.emit({ name = "emit_payload", msg = "hello", n = 7 })
    return captured
  )");
  COCONUT_REQUIRE(r.valid());
  sol::table captured = r;
  COCONUT_REQUIRE_EQ(captured["name"].get<std::string>(), "emit_payload");
  COCONUT_REQUIRE_EQ(captured["msg"].get<std::string>(), "hello");
  COCONUT_REQUIRE_EQ(captured["n"].get<int>(), 7);
}

COCONUT_TEST(unit, event_emit_errors_without_name) {
  EventTestFixture f;

  auto r = f.lua->safe_script(R"(
    local ok, err = pcall(coconut.emit, { msg = "no name" })
    return { ok = ok, err = tostring(err) }
  )", sol::script_pass_on_error);
  COCONUT_REQUIRE(r.valid());
  sol::table t = r;
  COCONUT_REQUIRE(not t["ok"].get<bool>());
}

COCONUT_TEST(unit, event_emit_errors_with_non_table) {
  EventTestFixture f;

  auto r = f.lua->safe_script(R"(
    local ok, err = pcall(coconut.emit, "string")
    return { ok = ok, err = tostring(err) }
  )", sol::script_pass_on_error);
  COCONUT_REQUIRE(r.valid());
  sol::table t = r;
  COCONUT_REQUIRE(not t["ok"].get<bool>());
}

// ── No-tier dispatch (no subscribers registered) ────────────────────

COCONUT_TEST(unit, event_dispatch_no_listeners_no_crash) {
  EventTestFixture f;

  auto r = f.script(R"(
    local e = coconut._dispatch("orphan", { some = "data" }, "")
    return e.name
  )");
  COCONUT_REQUIRE(r.valid());
  COCONUT_REQUIRE_EQ(r.get<std::string>(), "orphan");
}

COCONUT_TEST(unit, event_dispatch_default_events_is_noop) {
  EventTestFixture f;

  // The default coconut.events is a no-op that takes (event).
  // Verify it doesn't error when invoked by _dispatch.
  auto r = f.script(R"(
    coconut.on("safe", function(e) end)
    local e = coconut._dispatch("safe", {}, "")
    return e.name
  )");
  COCONUT_REQUIRE(r.valid());
  COCONUT_REQUIRE_EQ(r.get<std::string>(), "safe");
}

// ── View-scoped events via view:on() ────────────────────────────────

COCONUT_TEST(unit, event_view_on_stores_callback) {
  EventTestFixture f;

  auto r = f.script(R"(
    local desc = View.html("<html></html>")
    local ret = desc:on("ev", function(e) end)
    -- Chainable: returns self
    return desc._callbacks["ev"] ~= nil and ret == desc
  )");
  COCONUT_REQUIRE(r.valid());
  COCONUT_REQUIRE(r.get<bool>());
}

COCONUT_TEST(unit, event_view_on_overwrites_previous) {
  EventTestFixture f;

  auto r = f.script(R"(
    local desc = View.html("<html></html>")
    desc:on("ev", function(e) return "old" end)
    desc:on("ev", function(e) return "new" end)
    return desc._callbacks["ev"]({ name = "ev" })
  )");
  COCONUT_REQUIRE(r.valid());
  COCONUT_REQUIRE_EQ(r.get<std::string>(), "new");
}

COCONUT_TEST(unit, event_view_on_fires_in_tier1) {
  EventTestFixture f;

  auto r = f.script(R"(
    local order = {}
    local view = View.html("<html></html>")
    view:on("vt1", function(e) table.insert(order, "view") end)

    coconut._view_descriptors = coconut._view_descriptors or {}
    coconut._view_descriptors["v"] = view
    coconut._active_view = "v"

    coconut.on("vt1", function(e) table.insert(order, "sub") end)
    coconut.events = function(e) table.insert(order, "fb") end

    coconut._dispatch("vt1", {}, "v")
    return order
  )");
  COCONUT_REQUIRE(r.valid());
  sol::table order = r;
  COCONUT_REQUIRE_EQ(order[1].get<std::string>(), "view");
  COCONUT_REQUIRE_EQ(order[2].get<std::string>(), "sub");
  COCONUT_REQUIRE_EQ(order[3].get<std::string>(), "fb");
}

COCONUT_TEST(unit, event_view_on_stop_propagation_skips_tier23) {
  EventTestFixture f;

  auto r = f.script(R"(
    local order = {}
    local view = View.html("<html></html>")
    view:on("sp23", function(e) table.insert(order, "view"); e:stopPropagation() end)

    coconut._view_descriptors = coconut._view_descriptors or {}
    coconut._view_descriptors["v"] = view
    coconut._active_view = "v"

    coconut.on("sp23", function(e) table.insert(order, "sub") end)
    coconut.events = function(e) table.insert(order, "fb") end

    coconut._dispatch("sp23", {}, "v")
    return order
  )");
  COCONUT_REQUIRE(r.valid());
  sol::table order = r;
  COCONUT_REQUIRE_EQ(order[1].get<std::string>(), "view");
  COCONUT_REQUIRE_EQ(order.size(), 1);
}

// ── Event object passed to listeners has expected shape ────────────

COCONUT_TEST(unit, event_listener_receives_full_event_object) {
  EventTestFixture f;

  auto r = f.script(R"(
    local captured = nil
    coconut.on("shape", function(e) captured = e end)
    coconut._dispatch("shape", { a = 1, b = "two" }, "view1")
    return { name = captured.name, target = captured.target,
             a = captured.a, b = captured.b,
             has_preventDefault = type(captured.preventDefault) == "function" }
  )");
  COCONUT_REQUIRE(r.valid());
  sol::table t = r;
  COCONUT_REQUIRE_EQ(t["name"].get<std::string>(), "shape");
  COCONUT_REQUIRE_EQ(t["target"].get<std::string>(), "view1");
  COCONUT_REQUIRE_EQ(t["a"].get<int>(), 1);
  COCONUT_REQUIRE_EQ(t["b"].get<std::string>(), "two");
  COCONUT_REQUIRE(t["has_preventDefault"].get<bool>());
}

COCONUT_TEST(unit, event_type_getter_matches_name) {
  EventTestFixture f;

  // Access type directly from the event object without going through
  // a callback — metatable getter returns the event name.
  auto r = f.script(R"(
    local e = coconut._makeEvent("type_check", {}, "")
    return e.type
  )");
  COCONUT_REQUIRE(r.valid());
  COCONUT_REQUIRE_EQ(r.get<std::string>(), "type_check");
}

COCONUT_TEST(unit, event_listener_receives_type_getter) {
  EventTestFixture f;

  auto r = f.script(R"(
    local captured = nil
    coconut.on("type_via_cb", function(e) captured = e.type end)
    coconut._dispatch("type_via_cb", {}, "")
    return captured
  )");
  COCONUT_REQUIRE(r.valid());
  COCONUT_REQUIRE_EQ(r.get<std::string>(), "type_via_cb");
}

// ── Playground / stress ─────────────────────────────────────────────

COCONUT_TEST(unit, event_dispatch_many_subscribers) {
  EventTestFixture f;

  auto r = f.script(R"(
    local count = 0
    for i = 1, 100 do
      coconut.on("big", function(e) count = count + 1 end)
    end
    coconut._dispatch("big", {}, "")
    return count
  )");
  COCONUT_REQUIRE(r.valid());
  COCONUT_REQUIRE_EQ(r.get<int>(), 100);
}

COCONUT_TEST(unit, event_on_unsubscribe_mid_dispatch) {
  EventTestFixture f;

  auto r = f.script(R"(
    local order = {}
    local unsub = nil
    unsub = coconut.on("mid", function(e)
      table.insert(order, "first")
      unsub()  -- Unsubscribe self
    end)
    coconut.on("mid", function(e) table.insert(order, "second") end)
    coconut._dispatch("mid", {}, "")
    -- Fire again to prove it was unsubscribed
    coconut._dispatch("mid", {}, "")
    return order
  )");
  COCONUT_REQUIRE(r.valid());
  sol::table order = r;
  // First dispatch: first + second
  // Second dispatch: second only
  COCONUT_REQUIRE_EQ(order.size(), 3);
  COCONUT_REQUIRE_EQ(order[1].get<std::string>(), "first");
  COCONUT_REQUIRE_EQ(order[2].get<std::string>(), "second");
  COCONUT_REQUIRE_EQ(order[3].get<std::string>(), "second");
}

// ── Lifecycle events (close, ready) ─────────────────────────────────

COCONUT_TEST(unit, event_close_dispatch_returns_event) {
  EventTestFixture f;

  auto r = f.script(R"(
    -- Dispatch a close event — _dispatch always returns the event.
    local e = coconut._dispatch("close", {}, "")
    return { name = e.name, has_preventDefault = type(e.preventDefault) == "function" }
  )");
  COCONUT_REQUIRE(r.valid());
  sol::table t = r;
  COCONUT_REQUIRE_EQ(t["name"].get<std::string>(), "close");
  COCONUT_REQUIRE(t["has_preventDefault"].get<bool>());
}

COCONUT_TEST(unit, event_close_cancelled_by_prevent_default) {
  EventTestFixture f;

  auto r = f.script(R"(
    coconut.on("close", function(e) e:preventDefault() end)
    local e = coconut._dispatch("close", {}, "")
    return e.defaultPrevented
  )");
  COCONUT_REQUIRE(r.valid());
  COCONUT_REQUIRE(r.get<bool>());
}

COCONUT_TEST(unit, event_close_not_cancelled_by_default) {
  EventTestFixture f;

  auto r = f.script(R"(
    local e = coconut._dispatch("close", {}, "")
    return e.defaultPrevented
  )");
  COCONUT_REQUIRE(r.valid());
  COCONUT_REQUIRE(not r.get<bool>());
}

COCONUT_TEST(unit, event_ready_dispatch_returns_event) {
  EventTestFixture f;

  auto r = f.script(R"(
    local e = coconut._dispatch("ready", {}, "")
    return { name = e.name, target = e.target }
  )");
  COCONUT_REQUIRE(r.valid());
  sol::table t = r;
  COCONUT_REQUIRE_EQ(t["name"].get<std::string>(), "ready");
  COCONUT_REQUIRE_EQ(t["target"].get<std::string>(), "");
}

COCONUT_TEST(unit, event_resize_dispatch_carries_dimensions) {
  EventTestFixture f;

  auto r = f.script(R"(
    -- Resize events carry w, h payload fields
    local captured = nil
    coconut.on("resize", function(e) captured = { w = e.w, h = e.h } end)
    coconut._dispatch("resize", { w = 1024, h = 768 }, "")
    return captured
  )");
  COCONUT_REQUIRE(r.valid());
  sol::table t = r;
  COCONUT_REQUIRE_EQ(t["w"].get<int>(), 1024);
  COCONUT_REQUIRE_EQ(t["h"].get<int>(), 768);
}

COCONUT_TEST(unit, event_focus_dispatch_carries_active_flag) {
  EventTestFixture f;

  auto r = f.script(R"(
    local captured = nil
    coconut.on("focus", function(e) captured = e.active end)
    coconut._dispatch("focus", { active = true }, "")
    return captured
  )");
  COCONUT_REQUIRE(r.valid());
  COCONUT_REQUIRE(r.get<bool>());
}

COCONUT_TEST(unit, event_blur_dispatch_carries_active_false) {
  EventTestFixture f;

  auto r = f.script(R"(
    local captured = nil
    coconut.on("focus", function(e) captured = e.active end)
    coconut._dispatch("focus", { active = false }, "")
    return captured
  )");
  COCONUT_REQUIRE(r.valid());
  COCONUT_REQUIRE(not r.get<bool>());
}
