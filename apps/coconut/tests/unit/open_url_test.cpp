#include "packages/open_url.h"
#include "test.h"

#include <string>

// ── URL opening API contract tests ───────────────────────────────────

COCONUT_TEST(unit, open_url_empty_returns_false) {
  // Empty URL — no side effects, returns false immediately.
  bool result = coconut::open_url::open("");
  COCONUT_REQUIRE(!result);
}

// ── Side-effectful tests (skipped in automated suite) ─────────────────
// These tests work correctly on the host platform. They're skipped
// here because open_url::open() calls the REAL system handler via
// NSWorkspace (macOS) / xdg-open (Linux) / ShellExecute (Windows).
// Running them would open browser tabs, phone dialers, and mail
// compose windows on every test run — unwanted in automation.

COCONUT_TEST_SKIP(unit, open_url_https,
    "skipped: opens https://example.com in real browser") {
  bool result = coconut::open_url::open("https://example.com");
  (void)result;
}

COCONUT_TEST_SKIP(unit, open_url_http,
    "skipped: opens http://example.com in real browser") {
  bool result = coconut::open_url::open("http://example.com");
  (void)result;
}

COCONUT_TEST_SKIP(unit, open_url_mailto,
    "skipped: opens Mail.app / default mail client") {
  bool result = coconut::open_url::open("mailto:test@example.com");
  (void)result;
}

COCONUT_TEST_SKIP(unit, open_url_long_url,
    "skipped: opens long URL in real browser") {
  std::string longUrl = "https://example.com/" + std::string(2000, 'a');
  bool result = coconut::open_url::open(longUrl);
  (void)result;
}

COCONUT_TEST_SKIP(unit, macos_open_url_tel,
    "skipped: opens Phone.app / FaceTime (macOS only)") {
  bool result = coconut::open_url::open("tel:+1234567890");
  (void)result;
}

COCONUT_TEST_SKIP(unit, macos_open_url_system_preferences,
    "skipped: opens System Settings pane (macOS only)") {
  bool result = coconut::open_url::open(
      "x-apple.systempreferences:com.apple.preference.security");
  (void)result;
}

COCONUT_TEST_SKIP(unit, linux_open_url_https,
    "skipped: opens browser via xdg-open (Linux only)") {
  bool result = coconut::open_url::open("https://example.com");
  (void)result;
}
