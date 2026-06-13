#include "error.h"
#include "test.h"

#include <expected>
#include <string>

// ── Error struct ──────────────────────────────────────────────────────

COCONUT_TEST(unit, error_default_constructible) {
  coconut::Error error{};
  COCONUT_REQUIRE(error.code == coconut::ErrorCode::Ok);
  COCONUT_REQUIRE(error.message.empty());
  COCONUT_REQUIRE(error.details.empty());
}

COCONUT_TEST(unit, error_full_init) {
  coconut::Error err{
    .code = coconut::ErrorCode::InvalidConfig,
    .message = "bad config",
    .details = "missing browser"
  };
  COCONUT_REQUIRE(err.code == coconut::ErrorCode::InvalidConfig);
  COCONUT_REQUIRE_EQ(err.message, std::string("bad config"));
  COCONUT_REQUIRE_EQ(err.details, std::string("missing browser"));
}

COCONUT_TEST(unit, error_message_only) {
  coconut::Error err{
    .code = coconut::ErrorCode::Unknown,
    .message = "something went wrong"
  };
  COCONUT_REQUIRE(err.code == coconut::ErrorCode::Unknown);
  COCONUT_REQUIRE(err.details.empty());
}

// ── All error codes ──────────────────────────────────────────────────

COCONUT_TEST(unit, error_code_values) {
  auto ok     = coconut::ErrorCode::Ok;
  auto unk    = coconut::ErrorCode::Unknown;
  auto ic     = coconut::ErrorCode::InvalidConfig;
  auto iv     = coconut::ErrorCode::InvalidView;
  auto mf     = coconut::ErrorCode::MissingFile;
  auto dc     = coconut::ErrorCode::DuplicateCommand;
  auto cn     = coconut::ErrorCode::CommandNotFound;
  auto ip     = coconut::ErrorCode::InvalidPayload;
  auto nr     = coconut::ErrorCode::NotReady;
  auto qo     = coconut::ErrorCode::QueueOverflow;
  auto le     = coconut::ErrorCode::LuaError;
  auto be     = coconut::ErrorCode::BridgeError;
  auto we     = coconut::ErrorCode::WebViewError;
  auto pe     = coconut::ErrorCode::ParseError;
  auto ie     = coconut::ErrorCode::IoError;
  auto ni     = coconut::ErrorCode::NotImplementedYet;

  // Verify they're all distinct
  COCONUT_REQUIRE(ok != unk);
  COCONUT_REQUIRE(unk != ic);
  COCONUT_REQUIRE(ic != iv);
  COCONUT_REQUIRE(iv != mf);
  COCONUT_REQUIRE(mf != dc);
  COCONUT_REQUIRE(dc != cn);
  COCONUT_REQUIRE(cn != ip);
  COCONUT_REQUIRE(ip != nr);
  COCONUT_REQUIRE(nr != qo);
  COCONUT_REQUIRE(qo != le);
  COCONUT_REQUIRE(le != be);
  COCONUT_REQUIRE(be != we);
  COCONUT_REQUIRE(we != pe);
  COCONUT_REQUIRE(pe != ie);
  COCONUT_REQUIRE(ie != ni);
}

COCONUT_TEST(unit, error_code_int_values) {
  COCONUT_REQUIRE_EQ(static_cast<int>(coconut::ErrorCode::Ok), 0);
  COCONUT_REQUIRE_EQ(static_cast<int>(coconut::ErrorCode::Unknown), 1);
  COCONUT_REQUIRE_EQ(static_cast<int>(coconut::ErrorCode::InvalidConfig), 2);
  COCONUT_REQUIRE_EQ(static_cast<int>(coconut::ErrorCode::InvalidView), 3);
  COCONUT_REQUIRE_EQ(static_cast<int>(coconut::ErrorCode::MissingFile), 4);
  COCONUT_REQUIRE_EQ(static_cast<int>(coconut::ErrorCode::DuplicateCommand), 5);
  COCONUT_REQUIRE_EQ(static_cast<int>(coconut::ErrorCode::CommandNotFound), 6);
  COCONUT_REQUIRE_EQ(static_cast<int>(coconut::ErrorCode::InvalidPayload), 7);
  COCONUT_REQUIRE_EQ(static_cast<int>(coconut::ErrorCode::NotReady), 8);
  COCONUT_REQUIRE_EQ(static_cast<int>(coconut::ErrorCode::QueueOverflow), 9);
  COCONUT_REQUIRE_EQ(static_cast<int>(coconut::ErrorCode::LuaError), 10);
  COCONUT_REQUIRE_EQ(static_cast<int>(coconut::ErrorCode::BridgeError), 11);
  COCONUT_REQUIRE_EQ(static_cast<int>(coconut::ErrorCode::WebViewError), 12);
  COCONUT_REQUIRE_EQ(static_cast<int>(coconut::ErrorCode::ParseError), 13);
  COCONUT_REQUIRE_EQ(static_cast<int>(coconut::ErrorCode::IoError), 14);
  COCONUT_REQUIRE_EQ(static_cast<int>(coconut::ErrorCode::NotImplementedYet), 15);
}

// ── std::expected integration ─────────────────────────────────────────

COCONUT_TEST(unit, error_expected_success) {
  std::expected<int, coconut::Error> result = 42;
  COCONUT_REQUIRE(result.has_value());
  COCONUT_REQUIRE_EQ(result.value(), 42);
}

COCONUT_TEST(unit, error_expected_failure) {
  std::expected<int, coconut::Error> result =
    std::unexpected(coconut::Error{
      .code = coconut::ErrorCode::MissingFile,
      .message = "file not found"
    });
  COCONUT_REQUIRE(!result.has_value());
  COCONUT_REQUIRE_EQ(result.error().code, coconut::ErrorCode::MissingFile);
  COCONUT_REQUIRE_EQ(result.error().message, std::string("file not found"));
}

COCONUT_TEST(unit, error_expected_string_success) {
  std::expected<std::string, coconut::Error> result = std::string("hello");
  COCONUT_REQUIRE(result.has_value());
  COCONUT_REQUIRE_EQ(result.value(), std::string("hello"));
}

COCONUT_TEST(unit, error_expected_string_failure) {
  std::expected<std::string, coconut::Error> result =
    std::unexpected(coconut::Error{
      .code = coconut::ErrorCode::NotImplementedYet
    });
  COCONUT_REQUIRE(!result.has_value());
  COCONUT_REQUIRE_EQ(result.error().code, coconut::ErrorCode::NotImplementedYet);
}
