#include "config.h"
#include "test.h"
#include "window.h"

extern "C" {
#include <webui.h>
}

COCONUT_TEST(unit, webui_window_create_and_destroy) {
  coconut::Config config{};

  size_t win_id = webui_new_window();
  COCONUT_REQUIRE(win_id > 0);
  auto result = coconut::window::createWindow(&config, win_id);
  COCONUT_REQUIRE(result);
  coconut::window::Window* window = result.value();

  COCONUT_REQUIRE(window != nullptr);
  COCONUT_REQUIRE(window->configs == &config);
  COCONUT_REQUIRE(window->views.empty());

  coconut::window::destroyWindow(window);
}
