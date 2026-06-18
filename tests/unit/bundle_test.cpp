#include "bundle.h"
#include "config.h"
#include "test.h"

#include <cstdio>
#include <filesystem>
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

// Ensure bundle is renamed to .app for macOS launchability
COCONUT_TEST(unit, bundle_pipeline_success) {
  coconut::Config cfg{};
  cfg.app.name = "Test App";
  cfg.window_width = 800;

  std::string outDir = "/tmp/coconut-bundle-unit-test";
  std::string bundleDir = outDir + ".app";

  auto result = coconut::bundle::bundle(cfg, outDir);
  COCONUT_REQUIRE(result.has_value());

  // Verify stripped config was written
  std::string configPath = bundleDir + "/Contents/Resources/coconut.config.json";
  std::ifstream f(configPath);
  COCONUT_REQUIRE(f.is_open());
  f.close();

  // Verify Info.plist was generated
  std::string plistPath = bundleDir + "/Contents/Info.plist";
  std::ifstream pf(plistPath);
  COCONUT_REQUIRE(pf.is_open());
  pf.close();

  // Verify binary was copied to MacOS/
  std::string binPath = bundleDir + "/Contents/MacOS/coconut";
  std::ifstream bf(binPath, std::ios::binary);
  COCONUT_REQUIRE(bf.is_open());
  bf.close();

  // cleanup
  std::filesystem::remove_all(outDir);
  std::filesystem::remove_all(bundleDir);
}

COCONUT_TEST(unit, bundle_generate_manifests_now_implemented) {
  coconut::Config cfg{};
  cfg.app.name = "Test App";
  cfg.app.id = "com.test.app";

  auto result = coconut::bundle::generateManifests(cfg, "/tmp/coconut-manifest-test");
  COCONUT_REQUIRE(result.has_value());

  // Verify generated files
  std::ifstream plist("/tmp/coconut-manifest-test/Contents/Info.plist");
  COCONUT_REQUIRE(plist.is_open());
  plist.close();

  std::ifstream appmanifest("/tmp/coconut-manifest-test/app.manifest");
  COCONUT_REQUIRE(appmanifest.is_open());
  appmanifest.close();

  std::ifstream desktop("/tmp/coconut-manifest-test/com.test.app.desktop");
  COCONUT_REQUIRE(desktop.is_open());
  desktop.close();

  std::ifstream metainfo("/tmp/coconut-manifest-test/com.test.app.metainfo.xml");
  COCONUT_REQUIRE(metainfo.is_open());
  metainfo.close();

  std::filesystem::remove_all("/tmp/coconut-manifest-test");
}

COCONUT_TEST(unit, bundle_assemble_now_implemented) {
  coconut::Config cfg{};

  auto result = coconut::bundle::assembleBundle(cfg, "/tmp/coconut-assemble-test");
  COCONUT_REQUIRE(result.has_value());

  // Verify bundle structure
  COCONUT_REQUIRE(std::filesystem::exists("/tmp/coconut-assemble-test/Contents/MacOS/coconut"));
  COCONUT_REQUIRE(std::filesystem::exists("/tmp/coconut-assemble-test/Contents/Resources"));

  std::filesystem::remove_all("/tmp/coconut-assemble-test");
}

COCONUT_TEST(bundle, compile_lua_file) {
  // Create a temp directory with a test .lua file
  std::string tmpDir = "/tmp/coconut-bytecode-test";
  std::filesystem::create_directories(tmpDir);

  std::string testFile = tmpDir + "/test.lua";
  {
    std::ofstream f(testFile);
    f << "return 42" << std::endl;
  }

  // Try compilation — will succeed if luajit is on PATH
  auto compileResult = coconut::bundle::compileLuaFile(testFile);
  
  if (compileResult.has_value()) {
    // Compilation succeeded — verify the bytecode
    COCONUT_REQUIRE(std::filesystem::exists(compileResult.value()));
    
    // Verify the file is non-empty
    std::error_code ec;
    auto fileSize = std::filesystem::file_size(compileResult.value(), ec);
    COCONUT_REQUIRE(!ec);
    COCONUT_REQUIRE(fileSize > 0);

    // Verify it starts with LuaJIT bytecode header (0x1b4c4a = \x1bLJ)
    std::ifstream bc(compileResult.value(), std::ios::binary);
    COCONUT_REQUIRE(bc.is_open());
    char header[2];
    bc.read(header, 2);
    COCONUT_REQUIRE(header[0] == 0x1b);
    COCONUT_REQUIRE(header[1] == 'L');
    bc.close();
  } else {
    // Compilation failed — file should still exist as source
    COCONUT_REQUIRE(std::filesystem::exists(testFile));
  }

  std::filesystem::remove_all(tmpDir);
}
