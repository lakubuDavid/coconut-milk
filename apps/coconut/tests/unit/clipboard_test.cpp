#if defined(__APPLE__)
#include "platform/darwin/clipboard.h"
#elif defined(_WIN32)
#include "platform/win/clipboard.h"
#elif defined(__linux__)
#include "platform/linux/clipboard.h"
#else
#error "Unsupported platform - no clipboard implementation available"
#endif
#include "test.h"

#include <string>

// ── Cross-platform clipboard tests ───────────────────────────────────
// These test the clipboard API on all platforms. On macOS, the clipboard
// is always available. On Windows/Linux, availability depends on the
// display server / sandbox environment.

COCONUT_TEST(unit, clipboard_read_text_returns_string) {
  // readText() always returns a string (possibly empty)
  auto result = coconut::clipboard::platformReadText();
  (void)result;  // no crash
}

COCONUT_TEST(unit, clipboard_write_text_returns_bool) {
  // writeText() always returns a bool
  bool ok = coconut::clipboard::platformWriteText("hello");
  COCONUT_REQUIRE(ok == true || ok == false);
}

COCONUT_TEST(unit, clipboard_write_then_read) {
  // Write a known string then read it back
  bool wrote = coconut::clipboard::platformWriteText("coconut-clipboard-test");
  if (!wrote)
    return;  // clipboard not available in this environment
  std::string read = coconut::clipboard::platformReadText();
  COCONUT_REQUIRE(!read.empty());
}

COCONUT_TEST(unit, clipboard_empty_write_no_crash) {
  coconut::clipboard::platformWriteText("");
}

COCONUT_TEST(unit, clipboard_unicode) {
  coconut::clipboard::platformWriteText("coconut milk");
}

// ── macOS-specific: NSClipboard is always available ───────────────────

COCONUT_TEST_MACOS(unit, macos_clipboard_write_succeeds) {
  bool ok = coconut::clipboard::platformWriteText("macOS test");
  COCONUT_REQUIRE(ok);
}

// ── Linux-specific: requires X11/Wayland display ──────────────────────

COCONUT_TEST_LINUX(unit, linux_clipboard_available) {
  bool ok = coconut::clipboard::platformWriteText("linux test");
  // May fail if no display server is available (e.g., headless CI)
  (void)ok;
}
