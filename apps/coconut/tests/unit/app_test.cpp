#include "app.h"
#include "test.h"

#include <string>

// ── App lifecycle ─────────────────────────────────────────────────────

COCONUT_TEST(unit, app_create_and_destroy) {
  coconut::Config config{};

  auto app_result = coconut::app::create(&config);
  if (!app_result)
    return;  // skip: headless CI (no webview)
  coconut::App* app = app_result.value();

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

COCONUT_TEST(unit, app_destroy_null) {
  coconut::app::destroy(nullptr);
  // Must not crash
}

COCONUT_TEST(unit, app_create_null_config) {
  auto result = coconut::app::create(nullptr);
  COCONUT_REQUIRE(!result.has_value());
}

// ── App context ───────────────────────────────────────────────────────

COCONUT_TEST(unit, app_context_fields_set) {
  coconut::Config config{};
  config.view_root    = "views";
  config.asset_root   = "assets";
  config.command_root = "commands";

  auto app_result = coconut::app::create(&config);
  if (!app_result)
    return;  // skip: headless CI (no webview)
  coconut::App* app = app_result.value();

  COCONUT_REQUIRE(app->context != nullptr);
  COCONUT_REQUIRE(app->context->configs == &config);

  coconut::app::destroy(app);
}

COCONUT_TEST(unit, app_error_collection) {
  coconut::Config config{};
  auto            app_result = coconut::app::create(&config);
  if (!app_result)
    return;  // skip: headless CI (no webview)
  coconut::App* app = app_result.value();

  // Initially empty
  COCONUT_REQUIRE(app->errors.empty());

  // Add an error
  app->errors.push_back(coconut::Error{.code = coconut::ErrorCode::Unknown, .message = "test error"}
  );
  COCONUT_REQUIRE_EQ(app->errors.size(), size_t(1));
  COCONUT_REQUIRE_EQ(app->errors[0].message, std::string("test error"));

  coconut::app::destroy(app);
}

COCONUT_TEST(unit, app_context_configs_ptr) {
  coconut::Config config{};
  auto            app_result = coconut::app::create(&config);
  if (!app_result)
    return;  // skip: headless CI (no webview)
  coconut::App* app = app_result.value();

  // context->configs should point to the same config
  COCONUT_REQUIRE(app->context->configs == app->configs);
  COCONUT_REQUIRE(app->context->configs == &config);

  coconut::app::destroy(app);
}
