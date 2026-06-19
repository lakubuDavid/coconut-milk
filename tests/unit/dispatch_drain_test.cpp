/// Tests: dispatch::drain() processing of queued messages.
///
/// These verify that drain() handles each message kind correctly
/// without crashing when the App's subsystem pointers are null.

#include "app.h"
#include "dispatch.h"
#include "lua_runtime.h"
#include "test.h"

// Zeroed App — all subsystem pointers are null.
// drain() should handle this gracefully (no-op, no crash).

COCONUT_TEST(dispatch, drain_eval_js_with_null_webview) {
  coconut::App app{};

  coconut::dispatch::evalJS(&app, "console.log('test')");
  COCONUT_REQUIRE(!app.outbox.empty());

  // Must not crash even though webview is null.
  coconut::dispatch::drain(&app);
  COCONUT_REQUIRE(app.outbox.empty());
}

COCONUT_TEST(dispatch, drain_eval_js_multi_with_null_webview) {
  coconut::App app{};

  coconut::dispatch::evalJS(&app, "a");
  coconut::dispatch::evalJS(&app, "b");
  coconut::dispatch::evalJS(&app, "c");

  // Must not crash.
  coconut::dispatch::drain(&app);
  COCONUT_REQUIRE(app.outbox.empty());
}

COCONUT_TEST(dispatch, drain_lifecycle_with_null_lua) {
  coconut::App app{};

  coconut::dispatch::lifecycleEvent(&app, "workspace", "load");
  COCONUT_REQUIRE(!app.outbox.empty());

  // Must not crash even though lua_state is null.
  coconut::dispatch::drain(&app);
  COCONUT_REQUIRE(app.outbox.empty());
}

COCONUT_TEST(dispatch, drain_lifecycle_multi_with_null_lua) {
  coconut::App app{};

  coconut::dispatch::lifecycleEvent(&app, "v1", "load");
  coconut::dispatch::lifecycleEvent(&app, "v2", "mount");
  coconut::dispatch::lifecycleEvent(&app, "v1", "unmount");

  coconut::dispatch::drain(&app);
  COCONUT_REQUIRE(app.outbox.empty());
}

COCONUT_TEST(dispatch, drain_command_with_null_commands) {
  coconut::App app{};

  coconut::dispatch::commandCall(&app, "some_cmd", "{}");
  COCONUT_REQUIRE(!app.outbox.empty());

  // Must not crash even though commands registry is null.
  coconut::dispatch::drain(&app);
  COCONUT_REQUIRE(app.outbox.empty());
}

COCONUT_TEST(dispatch, drain_mixed_all_null) {
  coconut::App app{};

  coconut::dispatch::evalJS(&app, "js");
  coconut::dispatch::lifecycleEvent(&app, "v", "load");
  coconut::dispatch::commandCall(&app, "cmd", "{}");

  // All three kinds with null subsystems — no crash.
  coconut::dispatch::drain(&app);
  COCONUT_REQUIRE(app.outbox.empty());
}

COCONUT_TEST(dispatch, drain_null_app) {
  // drain(nullptr) must not crash.
  coconut::dispatch::drain(nullptr);
}

COCONUT_TEST(dispatch, drain_called_twice) {
  coconut::App app{};

  coconut::dispatch::evalJS(&app, "x");
  coconut::dispatch::drain(&app);
  COCONUT_REQUIRE(app.outbox.empty());

  // Second drain on empty queue.
  coconut::dispatch::drain(&app);
  COCONUT_REQUIRE(app.outbox.empty());
}

COCONUT_TEST(dispatch, drain_after_init_with_null_ptrs) {
  coconut::App app{};

  coconut::dispatch::init(&app);
  coconut::dispatch::evalJS(&app, "after_init");
  coconut::dispatch::lifecycleEvent(&app, "v", "load");
  coconut::dispatch::drain(&app);
  COCONUT_REQUIRE(app.outbox.empty());
  coconut::dispatch::shutdown(&app);
}
