#include "packages/open_url.h"
#include "test.h"

#include <string>

// ── Cross-platform URL opening tests ──────────────────────────────────

COCONUT_TEST(unit, open_url_empty_returns_false) {
  bool result = coconut::open_url::open("");
  COCONUT_REQUIRE(!result);
}

COCONUT_TEST(unit, open_url_https) {
  // HTTPS should ideally work on all desktop platforms
  bool result = coconut::open_url::open("https://example.com");
  // macOS: NSWorkspace handles this. Linux: xdg-open.
  // May fail in headless environments or if handler not available.
  (void)result;
}

COCONUT_TEST(unit, open_url_http) {
  bool result = coconut::open_url::open("http://example.com");
  (void)result;
}

COCONUT_TEST(unit, open_url_mailto) {
  // mailto: URLs are handled by the system mail client
  bool result = coconut::open_url::open("mailto:test@example.com");
  (void)result;
}

COCONUT_TEST(unit, open_url_long_url) {
  std::string longUrl = "https://example.com/" + std::string(2000, 'a');
  bool result = coconut::open_url::open(longUrl);
  (void)result;  // should not crash
}

// ── macOS-specific: NSWorkspace URL scheme support ────────────────────

COCONUT_TEST_MACOS(unit, macos_open_url_tel) {
  // tel: is handled by macOS via the Phone.app or FaceTime
  bool result = coconut::open_url::open("tel:+1234567890");
  COCONUT_REQUIRE(result);
}

COCONUT_TEST_MACOS(unit, macos_open_url_system_preferences) {
  // System Preferences deep links are macOS-specific
  bool result = coconut::open_url::open(
      "x-apple.systempreferences:com.apple.preference.security");
  COCONUT_REQUIRE(result);
}

// ── Linux-specific: xdg-open handler test ─────────────────────────────

COCONUT_TEST_LINUX(unit, linux_open_url_https) {
  // xdg-open should handle HTTPS on desktop Linux
  bool result = coconut::open_url::open("https://example.com");
  (void)result;  // may fail in headless CI
}
