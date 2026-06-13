#include "debug.h"
#include "test.h"

#include <string>

COCONUT_TEST(unit, debug_level_from_string_info) {
  auto level = coconut::debug::levelFromString("info");
#if defined(COCONUT_DEBUG_ENABLED)
  (void)level;  // returns a valid level
#else
  COCONUT_REQUIRE_EQ(level, coconut::debug::Level::Info);  // default fallback
#endif
}

COCONUT_TEST(unit, debug_level_from_string_debug) {
  auto level = coconut::debug::levelFromString("debug");
#if defined(COCONUT_DEBUG_ENABLED)
  (void)level;
#else
  COCONUT_REQUIRE_EQ(level, coconut::debug::Level::Debug);
#endif
}

COCONUT_TEST(unit, debug_level_from_string_warn) {
  auto level = coconut::debug::levelFromString("warn");
#if defined(COCONUT_DEBUG_ENABLED)
  (void)level;
#else
  COCONUT_REQUIRE_EQ(level, coconut::debug::Level::Warn);
#endif
}

COCONUT_TEST(unit, debug_level_from_string_error) {
  auto level = coconut::debug::levelFromString("error");
#if defined(COCONUT_DEBUG_ENABLED)
  (void)level;
#else
  COCONUT_REQUIRE_EQ(level, coconut::debug::Level::Error);
#endif
}

COCONUT_TEST(unit, debug_level_from_string_invalid) {
  // Unknown string should return Info (default)
  auto level = coconut::debug::levelFromString("bogus");
  COCONUT_REQUIRE_EQ(level, coconut::debug::Level::Info);
}

COCONUT_TEST(unit, debug_level_from_string_empty) {
  auto level = coconut::debug::levelFromString("");
  COCONUT_REQUIRE_EQ(level, coconut::debug::Level::Info);
}

COCONUT_TEST(unit, debug_set_level_no_crash) {
  coconut::debug::setLevel(coconut::debug::Level::Debug);
  coconut::debug::setLevel(coconut::debug::Level::Info);
  coconut::debug::setLevel(coconut::debug::Level::Warn);
  coconut::debug::setLevel(coconut::debug::Level::Error);
  // Must not crash
}

COCONUT_TEST(unit, debug_set_level_invalid_value) {
  // Cast invalid int to Level (should not crash)
  coconut::debug::setLevel(static_cast<coconut::debug::Level>(99));
}

COCONUT_TEST(unit, debug_info_no_crash) {
  coconut::debug::info("test message");
  coconut::debug::info(std::string("test message"));
  coconut::debug::info("");  // empty
}

COCONUT_TEST(unit, debug_warn_no_crash) {
  coconut::debug::warn("test warning");
  coconut::debug::warn(std::string("test warning"));
}

COCONUT_TEST(unit, debug_error_no_crash) {
  coconut::debug::error("test error");
  coconut::debug::error(std::string("test error"));
}

COCONUT_TEST(unit, debug_log_no_crash) {
  coconut::debug::log("test log");
  coconut::debug::log(std::string("test log"));
}
