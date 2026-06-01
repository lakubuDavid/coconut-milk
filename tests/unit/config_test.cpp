#include "config.h"
#include "test.h"

#include <string>

COCONUT_TEST(unit, config_defaults) {
  coconut::Config config{};

  COCONUT_REQUIRE_EQ(config.browser, std::string("auto"));
  COCONUT_REQUIRE_EQ(config.window_width, 1280);
  COCONUT_REQUIRE_EQ(config.window_height, 640);
  COCONUT_REQUIRE_EQ(config.initial_view, std::string("home"));
  COCONUT_REQUIRE_EQ(config.view_root, std::string("views"));
  COCONUT_REQUIRE_EQ(config.asset_root, std::string("assets"));
  COCONUT_REQUIRE_EQ(config.command_root, std::string("commands"));
}
