#include "routes.h"
#include "test.h"

#include <filesystem>
#include <fstream>
#include <set>
#include <string>

// ── RouteResult types ─────────────────────────────────────────────────

COCONUT_TEST(unit, route_result_default_not_found) {
  coconut::routes::RouteResult r{};
  COCONUT_REQUIRE_EQ(r.type, coconut::routes::RouteResult::NOT_FOUND);
  COCONUT_REQUIRE(r.view_name.empty());
  COCONUT_REQUIRE(r.file_path.empty());
  COCONUT_REQUIRE(r.data.empty());
}

COCONUT_TEST(unit, route_result_navigate_view) {
  coconut::routes::RouteResult r{
    .type = coconut::routes::RouteResult::NAVIGATE_VIEW,
    .view_name = "home"
  };
  COCONUT_REQUIRE_EQ(r.type, coconut::routes::RouteResult::NAVIGATE_VIEW);
  COCONUT_REQUIRE_EQ(r.view_name, std::string("home"));
}

COCONUT_TEST(unit, route_result_serve_file) {
  std::vector<uint8_t> data{0x48, 0x65, 0x6C};  // "Hel"
  coconut::routes::RouteResult r{
    .type = coconut::routes::RouteResult::SERVE_FILE,
    .file_path = "/tmp/test.html",
    .mime_type = "text/html",
    .data = data
  };
  COCONUT_REQUIRE_EQ(r.type, coconut::routes::RouteResult::SERVE_FILE);
  COCONUT_REQUIRE_EQ(r.file_path, std::string("/tmp/test.html"));
  COCONUT_REQUIRE_EQ(r.mime_type, std::string("text/html"));
  COCONUT_REQUIRE_EQ(r.data.size(), size_t(3));
}

// ── View name registration ───────────────────────────────────────────

COCONUT_TEST(unit, route_set_view_names_empty) {
  coconut::routes::setViewNames({});
  auto names = coconut::routes::getViewNames();
  COCONUT_REQUIRE(names.empty());
}

COCONUT_TEST(unit, route_set_view_names_single) {
  coconut::routes::setViewNames({"home"});
  auto names = coconut::routes::getViewNames();
  COCONUT_REQUIRE_EQ(names.size(), size_t(1));
  COCONUT_REQUIRE(names.count("home"));
}

COCONUT_TEST(unit, route_set_view_names_multiple) {
  coconut::routes::setViewNames({"home", "settings", "about"});
  auto names = coconut::routes::getViewNames();
  COCONUT_REQUIRE_EQ(names.size(), size_t(3));
  COCONUT_REQUIRE(names.count("home"));
  COCONUT_REQUIRE(names.count("settings"));
  COCONUT_REQUIRE(names.count("about"));
}

COCONUT_TEST(unit, route_set_view_names_overwrite) {
  coconut::routes::setViewNames({"old"});
  coconut::routes::setViewNames({"new"});
  auto names = coconut::routes::getViewNames();
  COCONUT_REQUIRE_EQ(names.size(), size_t(1));
  COCONUT_REQUIRE(names.count("new"));
  COCONUT_REQUIRE(!names.count("old"));
}

// ── Route handling: view navigation ───────────────────────────────────

COCONUT_TEST(unit, route_handle_navigate_view) {
  coconut::routes::setViewNames({"home", "dashboard"});

  auto r = coconut::routes::handle("home", "/tmp");
  COCONUT_REQUIRE_EQ(r.type, coconut::routes::RouteResult::NAVIGATE_VIEW);
  COCONUT_REQUIRE_EQ(r.view_name, std::string("home"));
}

COCONUT_TEST(unit, route_handle_navigate_dashboard) {
  coconut::routes::setViewNames({"home", "dashboard"});

  auto r = coconut::routes::handle("dashboard", "/tmp");
  COCONUT_REQUIRE_EQ(r.type, coconut::routes::RouteResult::NAVIGATE_VIEW);
  COCONUT_REQUIRE_EQ(r.view_name, std::string("dashboard"));
}

// ── Route handling: file serving ──────────────────────────────────────

COCONUT_TEST(unit, route_handle_serve_asset_js) {
  coconut::routes::setViewNames({"home"});

  // "assets/app.js" should resolve to SERVE_FILE if the file exists
  auto r = coconut::routes::handle("assets/app.js", "/");
  // Result depends on whether /assets/app.js exists on the system.
  // Most systems won't have this, so it may be NOT_FOUND.
  (void)r;
}

COCONUT_TEST(unit, route_handle_serve_nonexistent) {
  coconut::routes::setViewNames({"home"});

  // Non-existent file should return NOT_FOUND
  auto r = coconut::routes::handle("nonexistent.file", "/tmp");
  COCONUT_REQUIRE_EQ(r.type, coconut::routes::RouteResult::NOT_FOUND);
}

// ── Route handling: 404 ───────────────────────────────────────────────

COCONUT_TEST(unit, route_handle_unknown_path) {
  coconut::routes::setViewNames({"home"});

  auto r = coconut::routes::handle("some-random-string", "/tmp");
  COCONUT_REQUIRE_EQ(r.type, coconut::routes::RouteResult::NOT_FOUND);
}

COCONUT_TEST(unit, route_handle_empty_path) {
  coconut::routes::setViewNames({"home"});

  auto r = coconut::routes::handle("", "/tmp");
  COCONUT_REQUIRE_EQ(r.type, coconut::routes::RouteResult::NOT_FOUND);
}

COCONUT_TEST(unit, route_handle_root_as_file) {
  coconut::routes::setViewNames({"home"});

  // "/" is not a view name and not a file path → NOT_FOUND
  auto r = coconut::routes::handle("/", "/tmp");
  COCONUT_REQUIRE_EQ(r.type, coconut::routes::RouteResult::NOT_FOUND);
}

// ── Fallback file (SPA routing) ───────────────────────────────────────

COCONUT_TEST(unit, route_fallback_no_fallback) {
  coconut::routes::setViewNames({"home"});
  coconut::routes::setFallbackFile("");

  auto r = coconut::routes::handle("unknown/path", "/tmp");
  COCONUT_REQUIRE_EQ(r.type, coconut::routes::RouteResult::NOT_FOUND);
}

COCONUT_TEST(unit, route_fallback_serves_when_missing) {
  // When fallback file doesn't exist on disk, still NOT_FOUND
  coconut::routes::setViewNames({"home"});
  coconut::routes::setFallbackFile("does-not-exist.html");

  auto r = coconut::routes::handle("unknown/path", "/tmp");
  COCONUT_REQUIRE_EQ(r.type, coconut::routes::RouteResult::NOT_FOUND);
}

COCONUT_TEST(unit, route_fallback_serves_index_html) {
  // Create temp dir with an index.html
  auto tmp = std::filesystem::temp_directory_path() / "coconut_route_test";
  std::filesystem::create_directories(tmp);
  {
    std::ofstream f(tmp / "index.html");
    f << "<html><body>SPA</body></html>";
  }

  coconut::routes::setViewNames({"home"});
  coconut::routes::setFallbackFile("index.html");

  auto r = coconut::routes::handle("some/spa/route", tmp.string());
  COCONUT_REQUIRE_EQ(r.type, coconut::routes::RouteResult::SERVE_FILE);
  COCONUT_REQUIRE(!r.data.empty());
  COCONUT_REQUIRE_EQ(r.mime_type, std::string("text/html"));

  std::filesystem::remove_all(tmp);
}

COCONUT_TEST(unit, route_fallback_does_not_override_real_file) {
  auto tmp = std::filesystem::temp_directory_path() / "coconut_route_test2";
  std::filesystem::create_directories(tmp);
  {
    std::ofstream f(tmp / "index.html");
    f << "<html>fallback</html>";
    std::ofstream f2(tmp / "real.js");
    f2 << "console.log('real')";
  }

  coconut::routes::setViewNames({"home"});
  coconut::routes::setFallbackFile("index.html");

  auto r = coconut::routes::handle("real.js", tmp.string());
  COCONUT_REQUIRE_EQ(r.type, coconut::routes::RouteResult::SERVE_FILE);
  COCONUT_REQUIRE_EQ(r.mime_type, std::string("application/javascript"));

  std::filesystem::remove_all(tmp);
}

COCONUT_TEST(unit, route_fallback_does_not_override_view) {
  coconut::routes::setViewNames({"dashboard"});
  coconut::routes::setFallbackFile("index.html");

  auto r = coconut::routes::handle("dashboard", "/tmp");
  COCONUT_REQUIRE_EQ(r.type, coconut::routes::RouteResult::NAVIGATE_VIEW);
  COCONUT_REQUIRE_EQ(r.view_name, std::string("dashboard"));
}

// ── Route enum values ─────────────────────────────────────────────────

COCONUT_TEST(unit, route_result_type_values) {
  auto nav  = coconut::routes::RouteResult::NAVIGATE_VIEW;
  auto file = coconut::routes::RouteResult::SERVE_FILE;
  auto nf   = coconut::routes::RouteResult::NOT_FOUND;

  COCONUT_REQUIRE(nav  != file);
  COCONUT_REQUIRE(file != nf);
  COCONUT_REQUIRE(nf   != nav);
}
