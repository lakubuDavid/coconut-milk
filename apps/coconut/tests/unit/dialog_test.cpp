#include "dialog.h"
#include "test.h"

#include <expected>
#include <string>
#include <type_traits>
#include <vector>

// ── Dialog API contract tests ─────────────────────────────────────────
// These test the struct types and API surface, not the interactive UI.
// Dialog interactions require a window server and user input, so these
// only verify that the API contracts are valid and don't crash.

COCONUT_TEST(unit, dialog_result_default_constructible) {
  coconut::dialog::Result r{};
  COCONUT_REQUIRE(!r.confirmed);
  COCONUT_REQUIRE(r.path.empty());
  COCONUT_REQUIRE(r.paths.empty());
  COCONUT_REQUIRE(!r.is_dir);
}

COCONUT_TEST(unit, dialog_result_with_values) {
  coconut::dialog::Result r{
      .confirmed = true,
      .path      = "/tmp/test.txt",
      .paths     = {"/tmp/a.txt", "/tmp/b.txt"},
      .is_dir    = false
  };
  COCONUT_REQUIRE(r.confirmed);
  COCONUT_REQUIRE_EQ(r.path, std::string("/tmp/test.txt"));
  COCONUT_REQUIRE_EQ(r.paths.size(), size_t(2));
  COCONUT_REQUIRE_EQ(r.paths[0], std::string("/tmp/a.txt"));
  COCONUT_REQUIRE_EQ(r.paths[1], std::string("/tmp/b.txt"));
  COCONUT_REQUIRE(!r.is_dir);
}

COCONUT_TEST(unit, dialog_result_dir_flag) {
  coconut::dialog::Result r{.confirmed = true, .is_dir = true};
  COCONUT_REQUIRE(r.is_dir);
}

COCONUT_TEST(unit, dialog_filter_default_constructible) {
  coconut::dialog::Filter f{};
  COCONUT_REQUIRE(f.name.empty());
  COCONUT_REQUIRE(f.patterns.empty());
}

COCONUT_TEST(unit, dialog_filter_with_values) {
  coconut::dialog::Filter f{.name = "Images", .patterns = {"*.png", "*.jpg", "*.gif"}};
  COCONUT_REQUIRE_EQ(f.name, std::string("Images"));
  COCONUT_REQUIRE_EQ(f.patterns.size(), size_t(3));
  COCONUT_REQUIRE_EQ(f.patterns[0], std::string("*.png"));
  COCONUT_REQUIRE_EQ(f.patterns[1], std::string("*.jpg"));
  COCONUT_REQUIRE_EQ(f.patterns[2], std::string("*.gif"));
}

COCONUT_TEST(unit, dialog_filter_empty_patterns) {
  coconut::dialog::Filter f{.name = "All Files"};
  COCONUT_REQUIRE(f.patterns.empty());
}

// ── API surface compile check (no interactive tests in unit suite) ─────
// Dialog interactions require a window server and user input.
// These are tested manually or in integration tests.
// struct tests above verify the API contracts.

COCONUT_TEST(unit, dialog_api_returns_expected) {
  // Public API must return std::expected<Result, Error> so native failures
  // are values, not exceptions or silently-swallowed empty Results.
  using Expected = std::expected<coconut::dialog::Result, coconut::Error>;
  static_assert(std::is_same_v<decltype(coconut::dialog::messageBox("t", "m")), Expected>);
  static_assert(std::is_same_v<decltype(coconut::dialog::openFile("t")), Expected>);
  static_assert(std::is_same_v<decltype(coconut::dialog::saveFile("t")), Expected>);
  COCONUT_REQUIRE(true);
}

COCONUT_TEST(unit, dialog_error_is_dialog_code) {
  // An unexpected Error from a dialog failure carries ErrorCode::DialogError.
  coconut::Error err{
      .code = coconut::ErrorCode::DialogError, .message = "NSAlert failed", .details = "reason"
  };
  COCONUT_REQUIRE(err.code == coconut::ErrorCode::DialogError);
  COCONUT_REQUIRE_EQ(std::string(coconut::toString(err.code)), std::string("dialog_error"));
}
