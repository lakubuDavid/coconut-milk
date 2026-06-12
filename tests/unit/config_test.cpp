#include "config.h"
#include "error.h"
#include "test.h"

#include <cstdio>
#include <string>

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

// Helper: write a temp JSON file, run loadConfigJson on it, clean up.
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

COCONUT_TEST(unit, config_load_json_with_views) {
  const char* json = R"({
    "window_width": 1024,
    "initial_view": "dashboard",
    "views": {
      "home":   { "kind": "file", "src": "views/home.html" },
      "note":   { "kind": "file", "src": "views/note.html" },
      "about":  { "kind": "html", "src": "<h1>About</h1>" },
      "ext":    { "kind": "url",  "src": "https://example.com" }
    }
  })";

  auto result = loadFromString(json);
  COCONUT_REQUIRE(result.has_value());

  coconut::Config cfg = result.value();

  // Scalar overrides
  COCONUT_REQUIRE_EQ(cfg.window_width, 1024);
  COCONUT_REQUIRE_EQ(cfg.window_height, 640);   // kept default

  // Views map
  COCONUT_REQUIRE_EQ(cfg.views.size(), size_t(4));

  COCONUT_REQUIRE(cfg.views.count("home"));
  COCONUT_REQUIRE_EQ(cfg.views["home"].kind, std::string("file"));
  COCONUT_REQUIRE_EQ(cfg.views["home"].src, std::string("views/home.html"));

  COCONUT_REQUIRE(cfg.views.count("note"));
  COCONUT_REQUIRE_EQ(cfg.views["note"].kind, std::string("file"));
  COCONUT_REQUIRE_EQ(cfg.views["note"].src, std::string("views/note.html"));

  COCONUT_REQUIRE(cfg.views.count("about"));
  COCONUT_REQUIRE_EQ(cfg.views["about"].kind, std::string("html"));
  COCONUT_REQUIRE_EQ(cfg.views["about"].src, std::string("<h1>About</h1>"));

  COCONUT_REQUIRE(cfg.views.count("ext"));
  COCONUT_REQUIRE_EQ(cfg.views["ext"].kind, std::string("url"));
  COCONUT_REQUIRE_EQ(cfg.views["ext"].src, std::string("https://example.com"));
}

COCONUT_TEST(unit, config_load_json_invalid_view_kind) {
  const char* json = R"({
    "views": {
      "bad": { "kind": "ftp", "src": "something" }
    }
  })";

  auto result = loadFromString(json);
  COCONUT_REQUIRE(!result.has_value());
  COCONUT_REQUIRE_EQ(result.error().code, coconut::ErrorCode::InvalidConfig);
}

COCONUT_TEST(unit, config_load_json_missing_view_src) {
  const char* json = R"({
    "views": {
      "bad": { "kind": "file" }
    }
  })";

  auto result = loadFromString(json);
  COCONUT_REQUIRE(!result.has_value());
  COCONUT_REQUIRE_EQ(result.error().code, coconut::ErrorCode::InvalidConfig);
}

COCONUT_TEST(unit, config_load_json_missing_view_kind) {
  const char* json = R"({
    "views": {
      "bad": { "src": "views/bad.html" }
    }
  })";

  auto result = loadFromString(json);
  COCONUT_REQUIRE(!result.has_value());
  COCONUT_REQUIRE_EQ(result.error().code, coconut::ErrorCode::InvalidConfig);
}
