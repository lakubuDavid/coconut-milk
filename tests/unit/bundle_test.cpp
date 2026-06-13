#include "bundle.h"
#include "config.h"
#include "test.h"

#include <cstdio>
#include <fstream>
#include <string>

// ── stripConfig ───────────────────────────────────────────────────────

COCONUT_TEST(unit, bundle_strip_config_removes_debug) {
  coconut::Config cfg{};
  cfg.debug.enabled = true;
  cfg.debug.showTransportDump = true;
  cfg.debug.logLevel = "debug";

  coconut::Config stripped = coconut::stripConfig(cfg);
  COCONUT_REQUIRE(!stripped.debug.enabled);
  COCONUT_REQUIRE(!stripped.debug.showTransportDump);
  // logLevel falls back to "info" (DebugConfig default) — the important
  // thing is that the ORIGINAL "debug" value was stripped, not preserved.
  COCONUT_REQUIRE_EQ(stripped.debug.logLevel, std::string("info"));
}

COCONUT_TEST(unit, bundle_strip_config_removes_manifests) {
  coconut::Config cfg{};
  cfg.manifests.strip_dev_fields = true;
  cfg.manifests.bytecode_config = true;
  cfg.manifests.target_archs = {"x86_64", "arm64"};

  coconut::Config stripped = coconut::stripConfig(cfg);
  COCONUT_REQUIRE(stripped.manifests.strip_dev_fields);  // default is true
  COCONUT_REQUIRE(!stripped.manifests.bytecode_config);  // default is false
  COCONUT_REQUIRE(stripped.manifests.target_archs.empty());
}

COCONUT_TEST(unit, bundle_strip_config_preserves_app) {
  coconut::Config cfg{};
  cfg.app.name = "My App";
  cfg.app.id = "com.example.app";
  cfg.app.version = "1.0.0";

  coconut::Config stripped = coconut::stripConfig(cfg);
  COCONUT_REQUIRE_EQ(stripped.app.name, std::string("My App"));
  COCONUT_REQUIRE_EQ(stripped.app.id, std::string("com.example.app"));
  COCONUT_REQUIRE_EQ(stripped.app.version, std::string("1.0.0"));
}

COCONUT_TEST(unit, bundle_strip_config_preserves_window) {
  coconut::Config cfg{};
  cfg.window_width = 800;
  cfg.window_height = 600;
  cfg.resizable = false;
  cfg.frameless = true;
  cfg.transparent = true;

  coconut::Config stripped = coconut::stripConfig(cfg);
  COCONUT_REQUIRE_EQ(stripped.window_width, 800);
  COCONUT_REQUIRE_EQ(stripped.window_height, 600);
  COCONUT_REQUIRE(!stripped.resizable);
  COCONUT_REQUIRE(stripped.frameless);
  COCONUT_REQUIRE(stripped.transparent);
}

COCONUT_TEST(unit, bundle_strip_config_preserves_platform_ns) {
  coconut::Config cfg{};
  cfg.darwin.ns.notification_alert_style = "alert";
  cfg.darwin.ns.usage_descriptions["NSCameraUsageDescription"] = "Camera";

  coconut::Config stripped = coconut::stripConfig(cfg);
  COCONUT_REQUIRE_EQ(stripped.darwin.ns.notification_alert_style, std::string("alert"));
  COCONUT_REQUIRE_EQ(stripped.darwin.ns.usage_descriptions.size(), size_t(1));
}

COCONUT_TEST(unit, bundle_strip_config_removes_platform_manifests) {
  coconut::Config cfg{};
  cfg.darwin.manifests.bytecode_config = true;
  cfg.win.manifests.strip_dev_fields = false;

  coconut::Config stripped = coconut::stripConfig(cfg);
  COCONUT_REQUIRE(!stripped.darwin.manifests.bytecode_config);  // reset to default
  COCONUT_REQUIRE(stripped.win.manifests.strip_dev_fields);     // reset to default
}

COCONUT_TEST(unit, bundle_strip_config_preserves_icon) {
  coconut::Config cfg{};
  cfg.icon.icns_path = "icons/app.icns";
  cfg.icon.png_path = "icons/app.png";

  coconut::Config stripped = coconut::stripConfig(cfg);
  COCONUT_REQUIRE_EQ(stripped.icon.icns_path, std::string("icons/app.icns"));
  COCONUT_REQUIRE_EQ(stripped.icon.png_path, std::string("icons/app.png"));
}

COCONUT_TEST(unit, bundle_strip_config_preserves_views) {
  coconut::Config cfg{};
  cfg.views["home"] = coconut::ViewEntry{.kind = "file", .src = "views/home.html"};
  cfg.views["about"] = coconut::ViewEntry{.kind = "html", .src = "<h1>About</h1>"};

  coconut::Config stripped = coconut::stripConfig(cfg);
  COCONUT_REQUIRE_EQ(stripped.views.size(), size_t(2));
  COCONUT_REQUIRE_EQ(stripped.views["home"].src, std::string("views/home.html"));
  COCONUT_REQUIRE_EQ(stripped.views["about"].kind, std::string("html"));
}

// ── writeShippableConfig ─────────────────────────────────────────────

COCONUT_TEST(unit, bundle_write_shippable_config_creates_file) {
  coconut::Config cfg{};
  cfg.app.name = "Test";

  auto result = coconut::bundle::writeShippableConfig(cfg, "/tmp");
  COCONUT_REQUIRE(result.has_value());

  std::string path = result.value();
  COCONUT_REQUIRE(!path.empty());

  // Verify file exists
  std::ifstream f(path);
  COCONUT_REQUIRE(f.is_open());
  f.close();

  std::remove(path.c_str());
}

COCONUT_TEST(unit, bundle_write_shippable_config_invalid_dir) {
  coconut::Config cfg{};
  auto result = coconut::bundle::writeShippableConfig(cfg, "/nonexistent-dir/bundle");
  COCONUT_REQUIRE(!result.has_value());
  COCONUT_REQUIRE_EQ(result.error().code, coconut::ErrorCode::IoError);
}

// ── bundle pipeline ───────────────────────────────────────────────────

COCONUT_TEST(unit, bundle_pipeline_success) {
  coconut::Config cfg{};
  cfg.app.name = "Test App";
  cfg.window_width = 800;

  auto result = coconut::bundle::bundle(cfg, "/tmp/coconut-bundle-unit-test");
  COCONUT_REQUIRE(result.ok);

  // Verify stripped config was written
  std::string configPath = "/tmp/coconut-bundle-unit-test/coconut.config.json";
  std::ifstream f(configPath);
  COCONUT_REQUIRE(f.is_open());
  f.close();

  // Cleanup
  std::remove(configPath.c_str());
  std::remove("/tmp/coconut-bundle-unit-test");
}

COCONUT_TEST(unit, bundle_generate_manifests_scaffold) {
  coconut::Config cfg{};
  auto result = coconut::bundle::generateManifests(cfg, "/tmp");
  COCONUT_REQUIRE(!result.ok);  // scaffolded — not yet implemented
}

COCONUT_TEST(unit, bundle_assemble_scaffold) {
  coconut::Config cfg{};
  auto result = coconut::bundle::assembleBundle(cfg, "/tmp");
  COCONUT_REQUIRE(!result.ok);  // scaffolded — not yet implemented
}
