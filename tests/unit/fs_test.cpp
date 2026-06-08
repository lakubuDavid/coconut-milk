#include "config.h"
#include "fs.h"
#include "test.h"

#include <string>

COCONUT_TEST(unit, fs_roots_create_and_destroy) {
  coconut::Config config{};

  auto result = coconut::fs::create(&config);
  COCONUT_REQUIRE(result);
  coconut::fs::Roots* roots = result.value();

  COCONUT_REQUIRE(roots != nullptr);
  COCONUT_REQUIRE(roots->configs == &config);
  COCONUT_REQUIRE_EQ(roots->view_root, std::string("views"));
  COCONUT_REQUIRE_EQ(roots->asset_root, std::string("assets"));
  COCONUT_REQUIRE_EQ(roots->command_root, std::string("commands"));

  coconut::fs::destroy(roots);
}
