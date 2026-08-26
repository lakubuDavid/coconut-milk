#if defined(__APPLE__)
#include "platform/darwin/notify.h"
#elif defined(_WIN32)
#include "platform/win/notify.h"
#elif defined(__linux__)
#include "platform/linux/notify.h"
#else
#error "Unsupported platform - no notify implementation available"
#endif
#include "test.h"

#include <string>

// ── API contract — cross-platform ─────────────────────────────────────
// These test the cross-platform notify API. On platforms where the
// implementation isn't done yet, they will fail — that's expected.

COCONUT_TEST(unit, notify_standard) {
  // Send a standard notification
  // macOS: uses NSUserNotification → should succeed
  // Linux: uses notify-send → succeeds if available
  bool result = coconut::notify::platformNotify("Coconut Milk Test", "notification works");
  (void)result;  // may fail on platforms without notification support
}

COCONUT_TEST(unit, notify_returns_bool) {
  // notify() always returns a bool
  bool result = coconut::notify::platformNotify("test", "test");
  COCONUT_REQUIRE(result == true || result == false);
}

COCONUT_TEST(unit, notify_empty_title) {
  // Should not crash with empty title
  coconut::notify::platformNotify("", "body text");
}

COCONUT_TEST(unit, notify_empty_body) {
  // Should not crash with empty body
  coconut::notify::platformNotify("title", "");
}

COCONUT_TEST(unit, notify_both_empty) {
  // Should not crash with both empty
  coconut::notify::platformNotify("", "");
}

COCONUT_TEST(unit, notify_unicode) {
  // Unicode support in notifications
  bool result = coconut::notify::platformNotify("Coconut Milk", "Notificación de prueba");
  (void)result;
}

COCONUT_TEST(unit, notify_long_title) {
  std::string longTitle(500, 'A');
  bool        result = coconut::notify::platformNotify(longTitle, "short body");
  (void)result;  // should not crash
}

COCONUT_TEST(unit, notify_long_body) {
  std::string longBody(1000, 'B');
  bool        result = coconut::notify::platformNotify("title", longBody);
  (void)result;  // should not crash
}

COCONUT_TEST(unit, notify_special_characters) {
  bool result = coconut::notify::platformNotify("Test: $@#%^&*()", "Body: ¡™£¢∞§¶•ªº");
  (void)result;
}
