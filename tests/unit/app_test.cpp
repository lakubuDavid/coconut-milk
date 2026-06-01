#include "app.h"
#include "test.h"

COCONUT_TEST(unit, app_create_and_destroy) {
  coconut::Config config{};
  coconut::App *app = coconut::app::create(&config);

  COCONUT_REQUIRE(app != nullptr);
  COCONUT_REQUIRE(app->configs == &config);
  COCONUT_REQUIRE(app->context != nullptr);
  COCONUT_REQUIRE(app->context->configs == &config);
  COCONUT_REQUIRE(app->window == nullptr);
  COCONUT_REQUIRE(app->lua_state == nullptr);
  COCONUT_REQUIRE(app->bridge_state == nullptr);
  COCONUT_REQUIRE(app->commands == nullptr);
  COCONUT_REQUIRE(app->fs == nullptr);
  COCONUT_REQUIRE(app->errors.empty());

  coconut::app::destroy(app);
}
