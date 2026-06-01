#include "bridge.h"
#include "config.h"
#include "test.h"

COCONUT_TEST(unit, bridge_state_create_and_destroy) {
  coconut::Config config{};
  coconut::bridge::State *state = coconut::bridge::create(&config);

  COCONUT_REQUIRE(state != nullptr);
  COCONUT_REQUIRE(state->configs == &config);

  coconut::bridge::destroy(state);
}
