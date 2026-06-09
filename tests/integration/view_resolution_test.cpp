/// Tests for window::createView, addView, showView — view resolution and display.
///
/// Some of these assertions will *fail* until the corresponding features are
/// implemented, serving as a checklist of unfinished work.

#include "config.h"
#include "test.h"
#include "window.h"

#include <webview/webview.h>

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>

COCONUT_TEST(integration, create_file_view_reads_content) {
  // Write a temp HTML file to disk, then load it via createView.
  const char* tmp = "/tmp/_coconut_view_file_test.html";
  {
    std::ofstream f(tmp);
    COCONUT_REQUIRE(f.is_open());
    f << "<h1>Hello</h1>";
  }

  auto result = coconut::window::createView(tmp, coconut::window::VIEW_KIND_FILE,
                                            std::nullopt);
  COCONUT_REQUIRE(result.has_value());
  // The runtime is injected globally via webview_init() — no per-view injection.
  // File views now store the absolute path and serve via file:// navigation.
  COCONUT_REQUIRE(result->path.find("_coconut_view_file_test.html") != std::string::npos);

  std::remove(tmp);
}

COCONUT_TEST(integration, create_file_view_missing_file_fails) {
  auto result = coconut::window::createView("/nonexistent/file.html",
                                            coconut::window::VIEW_KIND_FILE,
                                            std::nullopt);
  COCONUT_REQUIRE(!result.has_value());
  COCONUT_REQUIRE_EQ(result.error().code, coconut::ErrorCode::MissingFile);
}

COCONUT_TEST(integration, create_html_view_stores_inline) {
  const std::string html = "<html><body>Hello</body></html>";
  auto result = coconut::window::createView(html, coconut::window::VIEW_KIND_HTML,
                                            std::nullopt);
  COCONUT_REQUIRE(result.has_value());
  // Runtime is injected globally via webview_init() — no per-view injection.
  COCONUT_REQUIRE(result->html.find("<html><body>Hello</body></html>") != std::string::npos);
}

COCONUT_TEST(integration, create_url_view_not_yet_implemented) {
  auto result = coconut::window::createView("https://example.com",
                                            coconut::window::VIEW_KIND_URL,
                                            std::nullopt);
  // URL views are not implemented — must return NotImplementedYet.
  COCONUT_REQUIRE(!result.has_value());
  COCONUT_REQUIRE_EQ(result.error().code, coconut::ErrorCode::NotImplementedYet);
}

COCONUT_TEST(integration, add_view_stores_in_window) {
  coconut::Config cfg{};
  webview_t wv = webview_create(0, NULL);
  COCONUT_REQUIRE(wv != nullptr);
  auto win = coconut::window::createWindow(&cfg, wv);
  COCONUT_REQUIRE(win.has_value());
  auto* w = win.value();

  auto view = coconut::window::createView("<p>test</p>", coconut::window::VIEW_KIND_HTML,
                                          std::nullopt);
  COCONUT_REQUIRE(view.has_value());
  auto* vp = new coconut::window::View(std::move(*view));

  coconut::window::addView(w, "test_view", vp);
  COCONUT_REQUIRE_EQ(w->views.size(), size_t(1));
  COCONUT_REQUIRE(w->views.count("test_view"));
  // No per-view runtime injection with webview.
  COCONUT_REQUIRE(w->views["test_view"]->html.rfind("<p>test</p>") != std::string::npos);

  coconut::window::destroyWindow(w);
  webview_destroy(wv);
}

COCONUT_TEST(integration, show_window_without_view_does_not_crash) {
  coconut::Config cfg{};
  webview_t wv = webview_create(0, NULL);
  COCONUT_REQUIRE(wv != nullptr);
  auto win = coconut::window::createWindow(&cfg, wv);
  COCONUT_REQUIRE(win.has_value());
  auto* w = win.value();

  // current_view is empty — showWindow should be a safe no-op or show placeholder.
  coconut::window::showWindow(w);

  coconut::window::destroyWindow(w);
  webview_destroy(wv);
}
