/// End-to-end test: full app lifecycle from config → window → context → lua.
///
/// Mirrors what main.cpp does.  Failures highlight what is still missing
/// in the startup chain.

#include "bridge.h"
#include "commands.h"
#include "config.h"
#include "context.h"
#include "fs.h"
#include "lua_runtime.h"
#include "test.h"
#include "window.h"

extern "C" {
#include <webui.h>
}

COCONUT_TEST(e2e, full_app_startup_lifecycle) {
  // 1. Config defaults
  coconut::Config cfg{};
  COCONUT_REQUIRE_EQ(cfg.browser, std::string("auto"));
  COCONUT_REQUIRE_EQ(cfg.initial_view, std::string("home"));

  // 2. Window
  size_t win_id = webui_new_window();
  COCONUT_REQUIRE(win_id > 0);
  auto win = coconut::window::createWindow(&cfg, win_id);
  COCONUT_REQUIRE(win.has_value());
  coconut::window::Window* window = win.value();
  COCONUT_REQUIRE(window->configs == &cfg);
  COCONUT_REQUIRE(window->views.empty());

  // 3. Context
  auto ctx = coconut::context::create(&cfg);
  COCONUT_REQUIRE(ctx.has_value());
  coconut::CoconutContext* context = ctx.value();
  context->window = window;
  COCONUT_REQUIRE(context->window == window);

  // 4. Lua runtime
  auto lua = coconut::lua::create(&cfg, context);
  COCONUT_REQUIRE(lua.has_value());
  coconut::lua::Runtime* lua_rt = lua.value();
  COCONUT_REQUIRE(lua_rt->lua_state != nullptr);
  COCONUT_REQUIRE((*lua_rt->lua_state)["coconut"].valid());
  COCONUT_REQUIRE((*lua_rt->lua_state)["ctx"].is<coconut::CoconutContext*>());

  // 5. Bridge state
  auto bridge_st = coconut::bridge::create(&cfg);
  COCONUT_REQUIRE(bridge_st.has_value());

  // 6. Command registry
  auto cmd_reg = coconut::commands::create(&cfg);
  COCONUT_REQUIRE(cmd_reg.has_value());

  // 7. FS roots
  auto fs_roots = coconut::fs::create(&cfg);
  COCONUT_REQUIRE(fs_roots.has_value());

  // 8. Teardown — reverse order, manual ownership.
  coconut::fs::destroy(fs_roots.value());
  coconut::commands::destroy(cmd_reg.value());
  coconut::bridge::destroy(bridge_st.value());
  coconut::lua::destroy(lua_rt);
  coconut::context::destroy(context);
  coconut::window::destroyWindow(window);
}
