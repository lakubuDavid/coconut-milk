#include "commands.h"
#include "test.h"

COCONUT_TEST(integration, commands_registry_placeholder) {
  coconut::commands::Registry registry{};
  (void)registry;
  COCONUT_REQUIRE(true);
}
