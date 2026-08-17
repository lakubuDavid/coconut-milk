#include "config.h"
#include "error.h"
#include "test.h"

#include <cstdio>
#include <string>

// ── Defaults ──────────────────────────────────────────────────────────

COCONUT_TEST(unit, config_defaults) {
  coconut::Config config{};

  COCONUT_REQUIRE_EQ(config.window_width, 1280);
  COCONUT_REQUIRE_EQ(config.window_height, 640);
  COCONUT_REQUIRE_EQ(config.initial_view, std::string("home"));
  COCONUT_REQUIRE_EQ(config.view_root, std::string("views"));
  COCONUT_REQUIRE_EQ(config.asset_root, std::string("assets"));
  COCONUT_REQUIRE_EQ(config.command_root, std::string("commands"));
  COCONUT_REQUIRE(config.views.empty());
}

COCONUT_TEST(unit, config_views_default_empty) {
  coconut::Config config{};
  COCONUT_REQUIRE(config.views.empty());
}

// ── AppConfig defaults ────────────────────────────────────────────────

COCONUT_TEST(unit, config_app_config_defaults) {
  coconut::AppConfig app{};
  COCONUT_REQUIRE(app.name.empty());
  COCONUT_REQUIRE(app.id.empty());
  COCONUT_REQUIRE(app.version.empty());
  COCONUT_REQUIRE(app.description.empty());
  COCONUT_REQUIRE(app.category.empty());
}

COCONUT_TEST(unit, config_app_config_set) {
  coconut::AppConfig app{};
  app.name = "Test App";
  app.id = "com.test.app";
  app.version = "1.0.0";
  app.description = "A test app";
  app.category = "development";

  COCONUT_REQUIRE_EQ(app.name, std::string("Test App"));
  COCONUT_REQUIRE_EQ(app.id, std::string("com.test.app"));
  COCONUT_REQUIRE_EQ(app.version, std::string("1.0.0"));
  COCONUT_REQUIRE_EQ(app.description, std::string("A test app"));
  COCONUT_REQUIRE_EQ(app.category, std::string("development"));
}

// ── IconConfig defaults ───────────────────────────────────────────────

COCONUT_TEST(unit, config_icon_defaults) {
  coconut::IconConfig icon{};
  COCONUT_REQUIRE(icon.icns_path.empty());
  COCONUT_REQUIRE(icon.ico_path.empty());
  COCONUT_REQUIRE(icon.png_path.empty());
}

COCONUT_TEST(unit, config_icon_set) {
  coconut::IconConfig icon{};
  icon.icns_path = "icons/app.icns";
  icon.ico_path = "icons/app.ico";
  icon.png_path = "icons/app.png";

  COCONUT_REQUIRE_EQ(icon.icns_path, std::string("icons/app.icns"));
  COCONUT_REQUIRE_EQ(icon.ico_path, std::string("icons/app.ico"));
  COCONUT_REQUIRE_EQ(icon.png_path, std::string("icons/app.png"));
}

// ── NsConfig defaults ─────────────────────────────────────────────────

COCONUT_TEST(unit, config_ns_defaults) {
  coconut::NsConfig ns{};
  COCONUT_REQUIRE(ns.notification_alert_style.empty());
  COCONUT_REQUIRE(ns.usage_descriptions.empty());
}

COCONUT_TEST(unit, config_ns_set) {
  coconut::NsConfig ns{};
  ns.notification_alert_style = "alert";
  ns.usage_descriptions["NSCameraUsageDescription"] = "Need camera";
  ns.usage_descriptions["NSMicrophoneUsageDescription"] = "Need mic";

  COCONUT_REQUIRE_EQ(ns.notification_alert_style, std::string("alert"));
  COCONUT_REQUIRE_EQ(ns.usage_descriptions.size(), size_t(2));
  COCONUT_REQUIRE_EQ(ns.usage_descriptions["NSCameraUsageDescription"],
                     std::string("Need camera"));
}

// ── ManifestsConfig defaults ──────────────────────────────────────────

COCONUT_TEST(unit, config_manifests_defaults) {
  coconut::ManifestsConfig m{};
  COCONUT_REQUIRE(m.strip_dev_fields);  // default true
  COCONUT_REQUIRE(!m.bytecode_config);  // default false
  COCONUT_REQUIRE(m.target_archs.empty());
  COCONUT_REQUIRE(m.darwin_info_plist_extra.empty());
  COCONUT_REQUIRE(m.darwin_entitlements.empty());
  COCONUT_REQUIRE(m.darwin_entitlements.empty());
  COCONUT_REQUIRE(m.linux_desktop_extra.empty());
  COCONUT_REQUIRE(m.linux_appstream.empty());
}

// ── PlatformConfig defaults ───────────────────────────────────────────

COCONUT_TEST(unit, config_platform_defaults) {
  coconut::PlatformConfig p{};
  COCONUT_REQUIRE(!p.frameless.has_value());
  COCONUT_REQUIRE(!p.transparent.has_value());
  COCONUT_REQUIRE(p.app.name.empty());       // no override
  COCONUT_REQUIRE(p.app.id.empty());
  COCONUT_REQUIRE(p.bundle_identifier.empty());
  COCONUT_REQUIRE(p.ns.notification_alert_style.empty());
  COCONUT_REQUIRE(p.manifests.strip_dev_fields);  // default true
}

// ── Config overrides ──────────────────────────────────────────────────

COCONUT_TEST(unit, config_override_frameless) {
  coconut::Config cfg{};
  cfg.frameless = true;
  COCONUT_REQUIRE(cfg.frameless);
}

COCONUT_TEST(unit, config_override_transparent) {
  coconut::Config cfg{};
  cfg.transparent = true;
  COCONUT_REQUIRE(cfg.transparent);
}

COCONUT_TEST(unit, config_override_window_dimensions) {
  coconut::Config cfg{};
  cfg.window_width = 800;
  cfg.window_height = 600;
  cfg.window_min_width = 400;
  cfg.window_min_height = 300;
  cfg.resizable = false;

  COCONUT_REQUIRE_EQ(cfg.window_width, 800);
  COCONUT_REQUIRE_EQ(cfg.window_height, 600);
  COCONUT_REQUIRE_EQ(cfg.window_min_width, 400);
  COCONUT_REQUIRE_EQ(cfg.window_min_height, 300);
  COCONUT_REQUIRE(!cfg.resizable);
}

// ── JSON load helper ─────────────────────────────────────────────────

static std::expected<coconut::Config, coconut::Error>
loadFromString(const std::string& json_text) {
  const char* tmp_path = "/tmp/_coconut_config_test.json";
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

// ── JSON load: views ──────────────────────────────────────────────────

COCONUT_TEST(unit, config_load_json_with_views) {
  const char* json = R"JSON({
    "window_width": 1024,
    "initial_view": "dashboard",
    "views": {
      "home":   { "kind": "file", "src": "views/home.html" },
      "note":   { "kind": "file", "src": "views/note.html" },
      "about":  { "kind": "html", "src": "<h1>About</h1>" },
      "ext":    { "kind": "url",  "src": "https://example.com" }
    }
  })JSON";

  auto result = loadFromString(json);
  COCONUT_REQUIRE(result.has_value());

  coconut::Config cfg = result.value();

  COCONUT_REQUIRE_EQ(cfg.window_width, 1024);
  COCONUT_REQUIRE_EQ(cfg.window_height, 640);   // kept default
  COCONUT_REQUIRE_EQ(cfg.views.size(), size_t(4));

  COCONUT_REQUIRE(cfg.views.count("home"));
  COCONUT_REQUIRE_EQ(cfg.views["home"].kind, std::string("file"));
  COCONUT_REQUIRE_EQ(cfg.views["home"].src, std::string("views/home.html"));
}

COCONUT_TEST(unit, config_load_json_invalid_view_kind) {
  const char* json = R"JSON({
    "views": {
      "bad": { "kind": "ftp", "src": "something" }
    }
  })JSON";

  auto result = loadFromString(json);
  COCONUT_REQUIRE(!result.has_value());
  COCONUT_REQUIRE_EQ(result.error().code, coconut::ErrorCode::InvalidConfig);
}

COCONUT_TEST(unit, config_load_json_missing_view_src) {
  const char* json = R"JSON({
    "views": {
      "bad": { "kind": "file" }
    }
  })JSON";

  auto result = loadFromString(json);
  COCONUT_REQUIRE(!result.has_value());
  COCONUT_REQUIRE_EQ(result.error().code, coconut::ErrorCode::InvalidConfig);
}

COCONUT_TEST(unit, config_load_json_missing_view_kind) {
  const char* json = R"JSON({
    "views": {
      "bad": { "src": "views/bad.html" }
    }
  })JSON";

  auto result = loadFromString(json);
  COCONUT_REQUIRE(!result.has_value());
  COCONUT_REQUIRE_EQ(result.error().code, coconut::ErrorCode::InvalidConfig);
}

// ── JSON load: app.* ──────────────────────────────────────────────────

COCONUT_TEST(unit, config_load_json_app_fields) {
  const char* json = R"JSON({
    "app": {
      "name": "Coconut Test",
      "id": "com.coconut.test",
      "version": "0.2.0",
      "description": "Test description",
      "category": "testing"
    }
  })JSON";

  auto result = loadFromString(json);
  COCONUT_REQUIRE(result.has_value());

  auto& app = result->app;
  COCONUT_REQUIRE_EQ(app.name, std::string("Coconut Test"));
  COCONUT_REQUIRE_EQ(app.id, std::string("com.coconut.test"));
  COCONUT_REQUIRE_EQ(app.version, std::string("0.2.0"));
  COCONUT_REQUIRE_EQ(app.description, std::string("Test description"));
  COCONUT_REQUIRE_EQ(app.category, std::string("testing"));
}

// ── JSON load: icon.* ─────────────────────────────────────────────────

COCONUT_TEST(unit, config_load_json_icon_fields) {
  const char* json = R"JSON({
    "icon": {
      "icns_path": "icons/app.icns",
      "ico_path": "icons/app.ico",
      "png_path": "icons/app.png"
    }
  })JSON";

  auto result = loadFromString(json);
  COCONUT_REQUIRE(result.has_value());

  auto& icon = result->icon;
  COCONUT_REQUIRE_EQ(icon.icns_path, std::string("icons/app.icns"));
  COCONUT_REQUIRE_EQ(icon.ico_path, std::string("icons/app.ico"));
  COCONUT_REQUIRE_EQ(icon.png_path, std::string("icons/app.png"));
}

// ── JSON load: manifests.* ────────────────────────────────────────────

COCONUT_TEST(unit, config_load_json_manifests_fields) {
  const char* json = R"JSON({
    "manifests": {
      "strip_dev_fields": false,
      "bytecode_config": true,
      "target_archs": ["x86_64", "arm64"],
      "darwin_info_plist_extra": {"NSAppleEventsUsageDescription": "Automation"},
      "darwin_entitlements": {"com.apple.security.app-sandbox": true}
    }
  })JSON";

  auto result = loadFromString(json);
  COCONUT_REQUIRE(result.has_value());

  auto& m = result->manifests;
  COCONUT_REQUIRE(!m.strip_dev_fields);
  COCONUT_REQUIRE(m.bytecode_config);
  COCONUT_REQUIRE_EQ(m.target_archs.size(), size_t(2));
  COCONUT_REQUIRE(m.darwin_info_plist_extra.contains("NSAppleEventsUsageDescription"));
}

// ── JSON load: darwin.* platform config ───────────────────────────────

COCONUT_TEST(unit, config_load_json_darwin_platform) {
  const char* json = R"JSON({
    "darwin": {
      "frameless": true,
      "transparent": true,
      "bundle_identifier": "com.coconut.test.darwin",
      "app": {
        "name": "Coconut Test (macOS)"
      },
      "ns": {
        "notification_alert_style": "alert",
        "usage_descriptions": {
          "NSCameraUsageDescription": "Camera for scanning",
          "NSMicrophoneUsageDescription": "Mic for recording"
        }
      },
      "manifests": {
        "bytecode_config": true
      }
    }
  })JSON";

  auto result = loadFromString(json);
  COCONUT_REQUIRE(result.has_value());

  auto& d = result->darwin;
  COCONUT_REQUIRE(d.frameless.has_value() && d.frameless.value());
  COCONUT_REQUIRE(d.transparent.has_value() && d.transparent.value());
  COCONUT_REQUIRE_EQ(d.bundle_identifier, std::string("com.coconut.test.darwin"));
  COCONUT_REQUIRE_EQ(d.app.name, std::string("Coconut Test (macOS)"));
  COCONUT_REQUIRE_EQ(d.ns.notification_alert_style, std::string("alert"));
  COCONUT_REQUIRE_EQ(d.ns.usage_descriptions.size(), size_t(2));
  COCONUT_REQUIRE(d.manifests.bytecode_config);
}

// ── JSON load: win.* platform config ──────────────────────────────────

COCONUT_TEST(unit, config_load_json_win_platform) {
  const char* json = R"JSON({
    "win": {
      "frameless": false,
      "app": {
        "id": "com.coconut.test.win"
      }
    }
  })JSON";

  auto result = loadFromString(json);
  COCONUT_REQUIRE(result.has_value());

  auto& w = result->win;
  COCONUT_REQUIRE(w.frameless.has_value() && !w.frameless.value());
  COCONUT_REQUIRE_EQ(w.app.id, std::string("com.coconut.test.win"));
}

// ── JSON load: linux.* platform config ────────────────────────────────

COCONUT_TEST(unit, config_load_json_linux_platform) {
  const char* json = R"JSON({
    "linux": {
      "app": {
        "category": "Development"
      }
    }
  })JSON";

  auto result = loadFromString(json);
  COCONUT_REQUIRE(result.has_value());
  COCONUT_REQUIRE_EQ(result->linux.app.category, std::string("Development"));
}

// ── JSON load: empty config ───────────────────────────────────────────

COCONUT_TEST(unit, config_load_json_empty) {
  auto result = loadFromString("{}");
  COCONUT_REQUIRE(result.has_value());
  // All defaults
  COCONUT_REQUIRE_EQ(result->window_width, 1280);
  COCONUT_REQUIRE_EQ(result->window_height, 640);
  COCONUT_REQUIRE(result->app.name.empty());
  COCONUT_REQUIRE(result->icon.icns_path.empty());
  COCONUT_REQUIRE(!result->darwin.frameless.has_value());
}

// ── JSON load: invalid JSON ───────────────────────────────────────────

COCONUT_TEST(unit, config_load_json_invalid_syntax) {
  auto result = loadFromString("{invalid json}");
  COCONUT_REQUIRE(!result.has_value());
  COCONUT_REQUIRE_EQ(result.error().code, coconut::ErrorCode::ParseError);
}

// ── Config error codes (from config.cpp) ──────────────────────────────

COCONUT_TEST(unit, config_error_missing_file) {
  auto result = coconut::loadConfigJson("/tmp/_nonexistent_config_file_test.json");
  COCONUT_REQUIRE(!result.has_value());
  COCONUT_REQUIRE_EQ(result.error().code, coconut::ErrorCode::MissingFile);
}

COCONUT_TEST(unit, config_error_invalid_view_kind_code) {
  const char* json = R"JSON({
    "views": {
      "v": { "kind": "unknown", "src": "" }
    }
  })JSON";
  auto result = loadFromString(json);
  COCONUT_REQUIRE(!result.has_value());
  COCONUT_REQUIRE_EQ(result.error().code, coconut::ErrorCode::InvalidConfig);
}

// ── Debug config defaults and loading ─────────────────────────────────

COCONUT_TEST(unit, config_debug_defaults) {
  coconut::Config cfg{};
  COCONUT_REQUIRE(!cfg.debug.enabled);
  COCONUT_REQUIRE(!cfg.debug.showTransportDump);
  COCONUT_REQUIRE_EQ(cfg.debug.logLevel, std::string("info"));
}

COCONUT_TEST(unit, config_load_json_debug) {
  const char* json = R"JSON({
    "debug": {
      "enabled": true,
      "showTransportDump": true,
      "logLevel": "debug"
    }
  })JSON";
  auto result = loadFromString(json);
  COCONUT_REQUIRE(result.has_value());

  auto& d = result->debug;
  COCONUT_REQUIRE(d.enabled);
  COCONUT_REQUIRE(d.showTransportDump);
  COCONUT_REQUIRE_EQ(d.logLevel, std::string("debug"));
}
