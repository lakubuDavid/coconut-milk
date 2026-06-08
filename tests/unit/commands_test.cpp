#include "commands.h"
#include "config.h"
#include "test.h"

COCONUT_TEST(unit, command_registry_create_and_destroy) {
  coconut::Config config{};

  auto result = coconut::commands::create(&config);
  COCONUT_REQUIRE(result);
  coconut::commands::Registry* registry = result.value();

  COCONUT_REQUIRE(registry != nullptr);
  COCONUT_REQUIRE(registry->configs == &config);
  COCONUT_REQUIRE(registry->handlers.empty());

  coconut::commands::destroy(registry);
}
