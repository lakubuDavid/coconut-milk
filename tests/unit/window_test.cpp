#include "test.h"
#include "window.h"

#include <filesystem>
#include <fstream>
#include <optional>

// ── View creation (does not require a webview handle) ─────────────────

COCONUT_TEST(unit, window_create_view_from_file) {
  // Create a temp file to satisfy the file-existence check
  auto tmp = std::filesystem::temp_directory_path() / "coconut_win_view_test.html";
  {
    std::ofstream f(tmp);
    f << "<h1>hello</h1>";
  }

  auto result = coconut::window::createView(tmp.string(),
                                             coconut::window::VIEW_KIND_FILE,
                                             std::nullopt);
  COCONUT_REQUIRE(result);
  auto view = result.value();
  COCONUT_REQUIRE_EQ(view.kind, coconut::window::VIEW_KIND_FILE);
  COCONUT_REQUIRE(view.path.find("coconut_win_view_test.html") != std::string::npos);

  std::error_code ec;
  std::filesystem::remove(tmp, ec);
}

COCONUT_TEST(unit, window_create_view_from_html) {
  auto result = coconut::window::createView("<h1>hello</h1>",
                                             coconut::window::VIEW_KIND_HTML,
                                             std::nullopt);
  COCONUT_REQUIRE(result);
  auto view = result.value();
  COCONUT_REQUIRE_EQ(view.kind, coconut::window::VIEW_KIND_HTML);
  COCONUT_REQUIRE_EQ(view.html, std::string("<h1>hello</h1>"));
}

COCONUT_TEST(unit, window_create_view_from_url) {
  auto result = coconut::window::createView("https://example.com",
                                             coconut::window::VIEW_KIND_URL,
                                             std::nullopt);
  COCONUT_REQUIRE(result);
  auto view = result.value();
  COCONUT_REQUIRE_EQ(view.kind, coconut::window::VIEW_KIND_URL);
  COCONUT_REQUIRE_EQ(view.path, std::string("https://example.com"));
}

COCONUT_TEST(unit, window_create_view_missing_file) {
  auto result = coconut::window::createView("/nonexistent/file.html",
                                             coconut::window::VIEW_KIND_FILE,
                                             std::nullopt);
  // Should fail with MissingFile error
  COCONUT_REQUIRE(!result.has_value());
  COCONUT_REQUIRE_EQ(result.error().code, coconut::ErrorCode::MissingFile);
}

// ── Window creation (requires webview_t — skipped without one) ───────

// Note: createWindow needs a non-null webview_t, which we don't have in
// a unit test. The window-tests that require a real webview are covered
// by the e2e suite (tests/integration/lua_bootstrap_test.cpp).

COCONUT_TEST(unit, window_create_null_webview_fails) {
  auto result = coconut::window::createWindow(nullptr, nullptr);
  COCONUT_REQUIRE(!result.has_value());
  COCONUT_REQUIRE_EQ(result.error().code, coconut::ErrorCode::WebViewError);
}

COCONUT_TEST(unit, window_destroy_null_noop) {
  coconut::window::destroyWindow(nullptr);
  // Must not crash
}

// ── Window operations (no webview) ───────────────────────────────────

// Show / hide / focus should gracefully handle null window or no webview.
// These are tested at the e2e level with a real App.

COCONUT_TEST(unit, window_destroy_with_views) {
  // Create a heap-allocated Window with views, destroy it
  coconut::window::Window* win = new coconut::window::Window();
  win->configs = nullptr;
  win->webview = nullptr;

  coconut::window::View* v1 = new coconut::window::View();
  v1->kind = coconut::window::VIEW_KIND_FILE;
  v1->path = "/test/file.html";

  win->views["test"] = v1;

  // destroyWindow should clean up views and delete the Window
  coconut::window::destroyWindow(win);
}

// ── View management ──────────────────────────────────────────────────

COCONUT_TEST(unit, window_add_and_show_view) {
  coconut::window::Window win{};
  win.configs = nullptr;
  win.webview = nullptr;

  coconut::window::View* view = new coconut::window::View();
  view->kind = coconut::window::VIEW_KIND_HTML;
  view->html = "<p>hello</p>";

  coconut::window::addView(&win, "greeting", view);
  COCONUT_REQUIRE(win.views.find("greeting") != win.views.end());
  COCONUT_REQUIRE(win.views["greeting"] == view);

  // showView on a null-webview window should not crash
  coconut::window::showView(&win, "greeting");

  // Clean up the view manually since we aren't calling destroyWindow
  delete view;
}

COCONUT_TEST(unit, window_add_null_view_noop) {
  coconut::window::Window win{};
  // Should not crash
  coconut::window::addView(&win, "nullview", nullptr);
}
