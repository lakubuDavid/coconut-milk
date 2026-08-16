#include "view_events.h"
#include "test.h"

// ── Lifecycle API contract tests ──────────────────────────────────────
// registerEvents/unregisterEvents require a fully initialized App with
// a running transport, so these test only that the API doesn't crash
// when called with null or after cleanup.

COCONUT_TEST(unit, lifecycle_unregister_without_register) {
  // Must not crash when unregistering without registration
  coconut::lifecycle::unregisterEvents();
}

COCONUT_TEST(unit, lifecycle_double_unregister) {
  // Must not crash when unregistering twice
  coconut::lifecycle::unregisterEvents();
  coconut::lifecycle::unregisterEvents();
}
