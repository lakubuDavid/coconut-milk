/// Tests: dispatch functions working with a zeroed-out App.
///
/// dispatch::evalJS / lifecycleEvent / commandCall / drain / init / shutdown
/// only access `app->outbox` and do null-pointer checks.  Everything else
/// on the App struct is ignored.  A zero-initialized App is sufficient.

#include "app.h"
#include "dispatch.h"
#include "test.h"

#include <string>
#include <string_view>
#include <vector>

/// Zero-initialized App — dispatch never touches other fields.
static coconut::App makeTestApp() {
  return coconut::App{};
}

// ── dispatch::evalJS ─────────────────────────────────────────────────

COCONUT_TEST(dispatch, eval_js_queues_to_app_outbox) {
  coconut::App app = makeTestApp();

  coconut::dispatch::evalJS(&app, "console.log('test')");

  COCONUT_REQUIRE(!app.outbox.empty());
  COCONUT_REQUIRE_EQ(app.outbox.size(), size_t(1));

  auto msg = app.outbox.pop();
  COCONUT_REQUIRE(msg.has_value());
  COCONUT_REQUIRE(msg->kind == coconut::dispatch::MessageKind::EvalJS);
  COCONUT_REQUIRE_EQ(msg->payload, std::string("console.log('test')"));
}

COCONUT_TEST(dispatch, eval_js_multi_push_fifo) {
  coconut::App app = makeTestApp();

  coconut::dispatch::evalJS(&app, "first");
  coconut::dispatch::evalJS(&app, "second");
  coconut::dispatch::evalJS(&app, "third");

  COCONUT_REQUIRE_EQ(app.outbox.size(), size_t(3));

  auto m1 = app.outbox.pop();
  auto m2 = app.outbox.pop();
  auto m3 = app.outbox.pop();
  COCONUT_REQUIRE(m1.has_value() && m2.has_value() && m3.has_value());
  COCONUT_REQUIRE_EQ(m1->payload, std::string("first"));
  COCONUT_REQUIRE_EQ(m2->payload, std::string("second"));
  COCONUT_REQUIRE_EQ(m3->payload, std::string("third"));
}

COCONUT_TEST(dispatch, eval_js_null_app) {
  // Must not crash.
  coconut::dispatch::evalJS(nullptr, "should not crash");
}

// ── dispatch::lifecycleEvent ─────────────────────────────────────────

COCONUT_TEST(dispatch, lifecycle_event_queues) {
  coconut::App app = makeTestApp();

  coconut::dispatch::lifecycleEvent(&app, "workspace", "load");

  auto msg = app.outbox.pop();
  COCONUT_REQUIRE(msg.has_value());
  COCONUT_REQUIRE(msg->kind == coconut::dispatch::MessageKind::LifecycleEvent);
  COCONUT_REQUIRE_EQ(msg->payload, std::string("workspace|load"));
}

COCONUT_TEST(dispatch, lifecycle_event_with_pipe_in_view_name) {
  coconut::App app = makeTestApp();

  coconut::dispatch::lifecycleEvent(&app, "my|view", "mount");

  auto msg = app.outbox.pop();
  COCONUT_REQUIRE(msg.has_value());
  COCONUT_REQUIRE_EQ(msg->payload, std::string("my|view|mount"));
}

COCONUT_TEST(dispatch, lifecycle_event_null_app) {
  coconut::dispatch::lifecycleEvent(nullptr, "v", "load");
}

// ── dispatch::commandCall ────────────────────────────────────────────

COCONUT_TEST(dispatch, command_call_queues) {
  coconut::App app = makeTestApp();

  coconut::dispatch::commandCall(&app, "open_file", R"({"path":"/tmp"})");

  auto msg = app.outbox.pop();
  COCONUT_REQUIRE(msg.has_value());
  COCONUT_REQUIRE(msg->kind == coconut::dispatch::MessageKind::CommandCall);
  COCONUT_REQUIRE_EQ(msg->payload, std::string(R"(open_file|{"path":"/tmp"})"));
}

COCONUT_TEST(dispatch, command_call_null_app) {
  coconut::dispatch::commandCall(nullptr, "cmd", "{}");
}

// ── dispatch::drain ──────────────────────────────────────────────────

COCONUT_TEST(dispatch, drain_empties_app_outbox) {
  coconut::App app = makeTestApp();

  coconut::dispatch::evalJS(&app, "a");
  coconut::dispatch::evalJS(&app, "b");
  coconut::dispatch::evalJS(&app, "c");
  COCONUT_REQUIRE_EQ(app.outbox.size(), size_t(3));

  coconut::dispatch::drain(&app);

  COCONUT_REQUIRE(app.outbox.empty());
  COCONUT_REQUIRE_EQ(app.outbox.size(), size_t(0));
}

COCONUT_TEST(dispatch, drain_empty_is_noop) {
  coconut::App app = makeTestApp();
  coconut::dispatch::drain(&app);
  COCONUT_REQUIRE(app.outbox.empty());
}

COCONUT_TEST(dispatch, drain_mixed_kinds) {
  coconut::App app = makeTestApp();

  coconut::dispatch::evalJS(&app, "js1");
  coconut::dispatch::lifecycleEvent(&app, "view", "load");
  coconut::dispatch::commandCall(&app, "cmd", "{}");
  coconut::dispatch::evalJS(&app, "js2");

  coconut::dispatch::drain(&app);
  COCONUT_REQUIRE(app.outbox.empty());
}

COCONUT_TEST(dispatch, drain_null_app) {
  coconut::dispatch::drain(nullptr);
}

COCONUT_TEST(dispatch, drain_multiple_calls) {
  coconut::App app = makeTestApp();

  coconut::dispatch::drain(&app);
  coconut::dispatch::drain(&app);

  coconut::dispatch::evalJS(&app, "x");
  coconut::dispatch::drain(&app);
  COCONUT_REQUIRE(app.outbox.empty());

  coconut::dispatch::evalJS(&app, "y");
  coconut::dispatch::drain(&app);
  COCONUT_REQUIRE(app.outbox.empty());
}

// ── dispatch::init / dispatch::shutdown ──────────────────────────────

COCONUT_TEST(dispatch, init_then_shutdown) {
  coconut::App app = makeTestApp();

  coconut::dispatch::init(&app);
  coconut::dispatch::evalJS(&app, "after_init");
  coconut::dispatch::drain(&app);
  coconut::dispatch::shutdown(&app);

  COCONUT_REQUIRE(app.outbox.empty());
}

COCONUT_TEST(dispatch, shutdown_drains) {
  coconut::App app = makeTestApp();

  coconut::dispatch::evalJS(&app, "one");
  coconut::dispatch::evalJS(&app, "two");

  coconut::dispatch::shutdown(&app);
  COCONUT_REQUIRE(app.outbox.empty());
}

COCONUT_TEST(dispatch, init_shutdown_null_app) {
  coconut::dispatch::init(nullptr);
  coconut::dispatch::shutdown(nullptr);
}

// ── Full cycle ───────────────────────────────────────────────────────

COCONUT_TEST(dispatch, full_cycle_zeroed_app) {
  coconut::App app = makeTestApp();

  coconut::dispatch::init(&app);

  coconut::dispatch::evalJS(&app, "js");
  coconut::dispatch::lifecycleEvent(&app, "v", "mount");
  coconut::dispatch::commandCall(&app, "cmd", R"({"k":"v"})");

  COCONUT_REQUIRE_EQ(app.outbox.size(), size_t(3));

  coconut::dispatch::drain(&app);
  COCONUT_REQUIRE(app.outbox.empty());

  coconut::dispatch::shutdown(&app);
}

// ── App struct has outbox ────────────────────────────────────────────

COCONUT_TEST(dispatch, app_struct_has_outbox) {
  coconut::App app = makeTestApp();
  COCONUT_REQUIRE(app.outbox.empty());
  COCONUT_REQUIRE_EQ(app.outbox.size(), size_t(0));
}

COCONUT_TEST(dispatch, app_outbox_usable) {
  coconut::App app = makeTestApp();

  bool ok = app.outbox.push({coconut::dispatch::MessageKind::EvalJS, "test"});
  COCONUT_REQUIRE(ok);
  COCONUT_REQUIRE(!app.outbox.empty());

  auto msg = app.outbox.pop();
  COCONUT_REQUIRE(msg.has_value());
}
