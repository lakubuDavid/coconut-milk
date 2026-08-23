/// Tests: dispatch module — thin main-loop pump semantics (Phase 2).
///
/// evalJS() routes through the transport's eval(); lifecycleEvent() queues
/// into the core Dispatcher; drain() flushes the Dispatcher. Legacy Outbox
/// behavior is gone; these tests cover the current contract and null-safety.

#include "app.h"
#include "dispatch.h"
#include "test.h"

#include <string>

static coconut::App makeTestApp() {
  return coconut::App{};
}

// ── dispatch::evalJS ─────────────────────────────────────────────────

COCONUT_TEST(dispatch, eval_js_null_app) {
  // Must not crash.
  coconut::dispatch::evalJS(nullptr, "should not crash");
}

// ── dispatch::lifecycleEvent ─────────────────────────────────────────

COCONUT_TEST(dispatch, lifecycle_event_null_app) {
  coconut::dispatch::lifecycleEvent(nullptr, "v", "load");
}

// ── dispatch::drain ──────────────────────────────────────────────────

COCONUT_TEST(dispatch, drain_null_app) {
  coconut::dispatch::drain(nullptr);
}

COCONUT_TEST(dispatch, drain_multiple_calls) {
  coconut::App app = makeTestApp();

  coconut::dispatch::drain(&app);
  coconut::dispatch::drain(&app);
}

// ── dispatch::init / dispatch::shutdown ──────────────────────────────

COCONUT_TEST(dispatch, init_then_shutdown) {
  coconut::App app = makeTestApp();

  coconut::dispatch::init(&app);
  coconut::dispatch::drain(&app);
  coconut::dispatch::shutdown(&app);
}

COCONUT_TEST(dispatch, shutdown_drains) {
  coconut::App app = makeTestApp();

  // shutdown() drains before tearing down — must not crash with no dispatcher.
  coconut::dispatch::shutdown(&app);
}

COCONUT_TEST(dispatch, init_shutdown_null_app) {
  coconut::dispatch::init(nullptr);
  coconut::dispatch::shutdown(nullptr);
}
