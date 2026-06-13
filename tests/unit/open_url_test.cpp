#include "packages/open_url.h"
#include "test.h"

#include <string>

// ── URL opening API contract tests ───────────────────────────────────
// NOTE: open_url::open() opens URLs in the real system handler (browser,
// mail app, phone dialer). Tests that would trigger side effects are
// excluded from the automated suite. Only the empty-URL contract test
// (which returns false without launching anything) is included here.

COCONUT_TEST(unit, open_url_empty_returns_false) {
  bool result = coconut::open_url::open("");
  COCONUT_REQUIRE(!result);
}

// Side-effectful tests (open_url_https, open_url_http, open_url_mailto,
// open_url_long_url, macos_open_url_tel, macos_open_url_system_preferences,
// linux_open_url_https) are intentionally excluded — they would open
// real system apps (browser, Mail.app, Phone.app, System Settings) on
// every test run. These belong in a manual / integration-only test suite
// that requires an explicit opt-in flag.
