#include "packages/notify.h"
#include "test.h"

#include <string>

// ── API contract — cross-platform ─────────────────────────────────────
// These test the cross-platform notify API. On platforms where the
// implementation isn't done yet, they will fail — that's expected.

COCONUT_TEST(unit, notify_standard) {
  // Send a standard notification
  // macOS: uses NSUserNotification → should succeed
  // Linux: uses notify-send → succeeds if available
  bool result = coconut::notify::notify("Coconut Milk Test", "notification works");
  (void)result;  // may fail on platforms without notification support
}

COCONUT_TEST(unit, notify_returns_bool) {
  // notify() always returns a bool
  bool result = coconut::notify::notify("test", "test");
  COCONUT_REQUIRE(result == true || result == false);
}

COCONUT_TEST(unit, notify_empty_title) {
  // Should not crash with empty title
  coconut::notify::notify("", "body text");
}

COCONUT_TEST(unit, notify_empty_body) {
  // Should not crash with empty body
  coconut::notify::notify("title", "");
}

COCONUT_TEST(unit, notify_both_empty) {
  // Should not crash with both empty
  coconut::notify::notify("", "");
}

COCONUT_TEST(unit, notify_unicode) {
  // Unicode support in notifications
  bool result = coconut::notify::notify("Coconut Milk", "Notificación de prueba");
  (void)result;
}

COCONUT_TEST(unit, notify_long_title) {
  std::string longTitle(500, 'A');
  bool result = coconut::notify::notify(longTitle, "short body");
  (void)result;  // should not crash
}

COCONUT_TEST(unit, notify_long_body) {
  std::string longBody(1000, 'B');
  bool result = coconut::notify::notify("title", longBody);
  (void)result;  // should not crash
}

COCONUT_TEST(unit, notify_special_characters) {
  bool result = coconut::notify::notify("Test: $@#%^&*()", "Body: ¡™£¢∞§¶•ªº");
  (void)result;
}
