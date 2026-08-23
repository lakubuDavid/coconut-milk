#include "bridge.h"
#include "common.h"
#include "config.h"
#include "test.h"

#include <nlohmann/json.hpp>
#include <string>

// ── Bridge state lifetime ─────────────────────────────────────────────

COCONUT_TEST(unit, bridge_state_create_and_destroy) {
  coconut::Config config{};

  auto result = coconut::bridge::create(&config);
  COCONUT_REQUIRE(result);
  coconut::bridge::State* state = result.value();

  COCONUT_REQUIRE(state != nullptr);
  COCONUT_REQUIRE(state->configs == &config);

  coconut::bridge::destroy(state);
}

COCONUT_TEST(unit, bridge_state_destroy_null) {
  coconut::bridge::destroy(nullptr);
  // Must not crash
}

COCONUT_TEST(unit, bridge_state_create_null_config) {
  auto result = coconut::bridge::create(nullptr);
  COCONUT_REQUIRE(!result.has_value());
}

// ── JS string escaping (consolidated into common::escapeString) ─────

COCONUT_TEST(unit, bridge_escape_js_single_quoted_empty) {
  auto result = coconut::common::escapeString("", '\'');
  COCONUT_REQUIRE(result.empty());
}

COCONUT_TEST(unit, bridge_escape_js_single_quoted_plain) {
  auto result = coconut::common::escapeString("hello", '\'');
  COCONUT_REQUIRE_EQ(result, std::string("hello"));
}

COCONUT_TEST(unit, bridge_escape_js_single_quoted_backslash) {
  auto result = coconut::common::escapeString("a\\b", '\'');
  COCONUT_REQUIRE_EQ(result, std::string("a\\\\b"));
}

COCONUT_TEST(unit, bridge_escape_js_single_quoted_quote) {
  auto result = coconut::common::escapeString("it's", '\'');
  COCONUT_REQUIRE_EQ(result, std::string("it\\'s"));
}

COCONUT_TEST(unit, bridge_escape_js_single_quoted_newline) {
  auto result = coconut::common::escapeString("a\nb", '\'');
  COCONUT_REQUIRE_EQ(result, std::string("a\\nb"));
}

COCONUT_TEST(unit, bridge_escape_js_single_quoted_carriage) {
  auto result = coconut::common::escapeString("a\rb", '\'');
  COCONUT_REQUIRE_EQ(result, std::string("a\\rb"));
}

COCONUT_TEST(unit, bridge_escape_js_single_quoted_tab) {
  auto result = coconut::common::escapeString("a\tb", '\'');
  COCONUT_REQUIRE_EQ(result, std::string("a\\tb"));
}

COCONUT_TEST(unit, bridge_escape_js_single_quoted_complex) {
  auto result = coconut::common::escapeString("it's a \"test\"\nwith\\backslash", '\'');
  COCONUT_REQUIRE_EQ(result, std::string("it\\'s a \"test\"\\nwith\\\\backslash"));
}

// ── JSON → Lua table conversion ──────────────────────────────────────

COCONUT_TEST(unit, bridge_json_to_table_null_json) {
  // null JSON string should produce empty Lua result
  // (depends on sol::state, which may not be available in test env)

  // For now, just test that the API doesn't crash with null/invalid input
  // This test is minimal until we have a proper Lua state in the test fixture
}

COCONUT_TEST(unit, bridge_escape_js_empty_string) {
  auto result = coconut::common::escapeString("", '\'');
  COCONUT_REQUIRE(result.empty());
}
