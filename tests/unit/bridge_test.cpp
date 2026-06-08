#include "bridge.h"
#include "config.h"
#include "test.h"

COCONUT_TEST(unit, bridge_state_create_and_destroy) {
  coconut::Config config{};

  auto result = coconut::bridge::create(&config);
  COCONUT_REQUIRE(result);
  coconut::bridge::State* state = result.value();

  COCONUT_REQUIRE(state != nullptr);
  COCONUT_REQUIRE(state->configs == &config);

  coconut::bridge::destroy(state);
}
