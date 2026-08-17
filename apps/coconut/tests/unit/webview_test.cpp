#include "config.h"
#include "test.h"
#include "window.h"

#include <webview/webview.h>

COCONUT_TEST(unit, webview_window_create_and_destroy) {
  coconut::Config config{};

  webview_t wv = webview_create(0, NULL);
  COCONUT_REQUIRE(wv != nullptr);
  auto result = coconut::window::createWindow(&config, wv);
  COCONUT_REQUIRE(result);
  coconut::window::Window* window = result.value();

  COCONUT_REQUIRE(window != nullptr);
  COCONUT_REQUIRE(window->configs == &config);
  COCONUT_REQUIRE(window->views.empty());

  // Clean up: window does NOT own the webview handle (App does).
  coconut::window::destroyWindow(window);
  webview_destroy(wv);
}
