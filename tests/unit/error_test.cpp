#include "error.h"
#include "test.h"

#include <string>

COCONUT_TEST(unit, error_struct_and_codes) {
  coconut::Error error{};

  COCONUT_REQUIRE(error.code == coconut::ErrorCode::Ok);
  COCONUT_REQUIRE(error.message.empty());
  COCONUT_REQUIRE(error.details.empty());

  coconut::Error invalid_config{.code = coconut::ErrorCode::InvalidConfig,
                                .message = "bad config",
                                .details = "missing browser"};

  COCONUT_REQUIRE(invalid_config.code == coconut::ErrorCode::InvalidConfig);
  COCONUT_REQUIRE_EQ(invalid_config.message, std::string("bad config"));
  COCONUT_REQUIRE_EQ(invalid_config.details, std::string("missing browser"));
}
