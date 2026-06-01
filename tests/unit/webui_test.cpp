#include "config.h"
#include "test.h"
#include "webui.h"

COCONUT_TEST(unit, webui_window_create_and_destroy) {
  coconut::Config config{};
  coconut::webui::Window *window = coconut::webui::create(&config);

  COCONUT_REQUIRE(window != nullptr);
  COCONUT_REQUIRE(window->configs == &config);
  COCONUT_REQUIRE(window->views.empty());

  coconut::webui::destroy(window);
}
