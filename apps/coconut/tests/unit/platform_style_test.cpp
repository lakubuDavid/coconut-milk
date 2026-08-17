#include "config.h"
#include "error.h"
#include "test.h"
#include "window.h"

#include <webview/webview.h>

#include <cstdio>
#include <string>

// ── Config parsing for frameless / transparent ────────────────────────

/// Helper: write a temp JSON file, run loadConfigJson on it, clean up.
static std::expected<coconut::Config, coconut::Error>
loadFromString(const std::string& json_text) {
  const char* tmp_path = "/tmp/_coconut_config_style_test.json";
  FILE* f = std::fopen(tmp_path, "w");
  if (!f) {
    return std::unexpected(
        coconut::Error{.code = coconut::ErrorCode::Unknown,
                       .message = "failed to write temp config"});
  }
  std::fwrite(json_text.data(), 1, json_text.size(), f);
  std::fclose(f);

  auto result = coconut::loadConfigJson(tmp_path);
  std::remove(tmp_path);
  return result;
}

// ── Platform dispatch behaviour (compile-time check) ────────────────

/// ApplyWindowStyle must accept any real Config combination without crashing.
///
/// On macOS this exercises the real ObjC NSWindow modifier code.
/// On other platforms the stubs are no-ops (verified by compilation only).
///
/// NOTE: These tests need a live webview window (macOS only in CI).
COCONUT_TEST(unit, platform_style_none) {
  coconut::Config cfg{};
  // Defaults: frameless=false, transparent=false

  webview_t wv = webview_create(0, NULL);
  COCONUT_REQUIRE(wv != nullptr);

  // Should not crash or warn — both flags are false.
  coconut::window::applyWindowStyle(nullptr);

  auto* window = coconut::window::createWindow(&cfg, wv).value();
  window->configs = &cfg;
  coconut::window::applyWindowStyle(window);

  coconut::window::destroyWindow(window);
  webview_destroy(wv);
}

COCONUT_TEST(unit, platform_style_frameless_only) {
  coconut::Config cfg{};
  cfg.frameless = true;
  cfg.transparent = false;

  webview_t wv = webview_create(0, NULL);
  COCONUT_REQUIRE(wv != nullptr);

  auto* window = coconut::window::createWindow(&cfg, wv).value();
  window->configs = &cfg;

  // Should set FullSizeContentView + hide title + hide traffic lights
  coconut::window::applyWindowStyle(window);

  coconut::window::destroyWindow(window);
  webview_destroy(wv);
}

COCONUT_TEST(unit, platform_style_transparent_only) {
  coconut::Config cfg{};
  cfg.frameless = false;
  cfg.transparent = true;

  webview_t wv = webview_create(0, NULL);
  COCONUT_REQUIRE(wv != nullptr);

  auto* window = coconut::window::createWindow(&cfg, wv).value();
  window->configs = &cfg;

  // Should set Opaque:NO + backgroundColor:clear + no shadow
  coconut::window::applyWindowStyle(window);

  coconut::window::destroyWindow(window);
  webview_destroy(wv);
}

COCONUT_TEST(unit, platform_style_frameless_and_transparent) {
  coconut::Config cfg{};
  cfg.frameless = true;
  cfg.transparent = true;

  webview_t wv = webview_create(0, NULL);
  COCONUT_REQUIRE(wv != nullptr);

  auto* window = coconut::window::createWindow(&cfg, wv).value();
  window->configs = &cfg;

  // Both blocks should execute without conflict
  coconut::window::applyWindowStyle(window);

  coconut::window::destroyWindow(window);
  webview_destroy(wv);
}

// ── Config default values (cross-platform, no webview needed) ────────

COCONUT_TEST(unit, config_style_defaults) {
  coconut::Config cfg{};

  COCONUT_REQUIRE_EQ(cfg.frameless, false);
  COCONUT_REQUIRE_EQ(cfg.transparent, false);

  // Mutate and verify
  cfg.frameless = true;
  cfg.transparent = true;
  COCONUT_REQUIRE_EQ(cfg.frameless, true);
  COCONUT_REQUIRE_EQ(cfg.transparent, true);

  // Each resets independently
  cfg.frameless = false;
  COCONUT_REQUIRE_EQ(cfg.frameless, false);
  COCONUT_REQUIRE_EQ(cfg.transparent, true);
}

// ── JSON config loading for frameless / transparent ────────────────

COCONUT_TEST(unit, config_style_json_load_frameless) {
  const char* json = R"({
    "frameless": true,
    "transparent": false
  })";

  auto result = loadFromString(json);
  COCONUT_REQUIRE(result.has_value());

  coconut::Config cfg = result.value();
  COCONUT_REQUIRE_EQ(cfg.frameless, true);
  COCONUT_REQUIRE_EQ(cfg.transparent, false);
}

COCONUT_TEST(unit, config_style_json_load_transparent) {
  const char* json = R"({
    "frameless": false,
    "transparent": true
  })";

  auto result = loadFromString(json);
  COCONUT_REQUIRE(result.has_value());

  coconut::Config cfg = result.value();
  COCONUT_REQUIRE_EQ(cfg.frameless, false);
  COCONUT_REQUIRE_EQ(cfg.transparent, true);
}

COCONUT_TEST(unit, config_style_json_load_both) {
  const char* json = R"({
    "frameless": true,
    "transparent": true
  })";

  auto result = loadFromString(json);
  COCONUT_REQUIRE(result.has_value());

  coconut::Config cfg = result.value();
  COCONUT_REQUIRE_EQ(cfg.frameless, true);
  COCONUT_REQUIRE_EQ(cfg.transparent, true);
}

COCONUT_TEST(unit, config_style_json_load_invalid_frameless) {
  // frameless must be a boolean
  const char* json = R"({
    "frameless": "yes"
  })";

  auto result = loadFromString(json);
  COCONUT_REQUIRE(!result.has_value());
  COCONUT_REQUIRE_EQ(result.error().code, coconut::ErrorCode::InvalidConfig);
}

COCONUT_TEST(unit, config_style_json_load_invalid_transparent) {
  // transparent must be a boolean
  const char* json = R"({
    "transparent": 1
  })";

  auto result = loadFromString(json);
  COCONUT_REQUIRE(!result.has_value());
  COCONUT_REQUIRE_EQ(result.error().code, coconut::ErrorCode::InvalidConfig);
}

// ── Lua config loading for frameless / transparent ─────────────────

/// Helper: write a temp .lua config, load it, clean up.
static std::expected<coconut::Config, coconut::Error>
loadLuaFromString(const std::string& lua_text) {
  const char* tmp_path = "/tmp/_coconut_lua_style_test.lua";
  FILE* f = std::fopen(tmp_path, "w");
  if (!f) {
    return std::unexpected(
        coconut::Error{.code = coconut::ErrorCode::Unknown,
                       .message = "failed to write temp lua config"});
  }
  std::fwrite(lua_text.data(), 1, lua_text.size(), f);
  std::fclose(f);

  auto result = coconut::loadConfigLua(tmp_path);
  std::remove(tmp_path);
  return result;
}

COCONUT_TEST(unit, config_style_lua_defaults) {
  const char* lua = R"(return {})";

  auto result = loadLuaFromString(lua);
  COCONUT_REQUIRE(result.has_value());

  coconut::Config cfg = result.value();
  // Defaults should match Config struct defaults
  COCONUT_REQUIRE_EQ(cfg.frameless, false);
  COCONUT_REQUIRE_EQ(cfg.transparent, false);
}

COCONUT_TEST(unit, config_style_lua_frameless) {
  const char* lua = R"(return { frameless = true, transparent = false })";

  auto result = loadLuaFromString(lua);
  COCONUT_REQUIRE(result.has_value());

  coconut::Config cfg = result.value();
  COCONUT_REQUIRE_EQ(cfg.frameless, true);
  COCONUT_REQUIRE_EQ(cfg.transparent, false);
}

COCONUT_TEST(unit, config_style_lua_transparent) {
  const char* lua = R"(return { frameless = false, transparent = true })";

  auto result = loadLuaFromString(lua);
  COCONUT_REQUIRE(result.has_value());

  coconut::Config cfg = result.value();
  COCONUT_REQUIRE_EQ(cfg.frameless, false);
  COCONUT_REQUIRE_EQ(cfg.transparent, true);
}

COCONUT_TEST(unit, config_style_lua_both) {
  const char* lua = R"(return { frameless = true, transparent = true })";

  auto result = loadLuaFromString(lua);
  COCONUT_REQUIRE(result.has_value());

  coconut::Config cfg = result.value();
  COCONUT_REQUIRE_EQ(cfg.frameless, true);
  COCONUT_REQUIRE_EQ(cfg.transparent, true);
}
