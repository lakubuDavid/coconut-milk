#include "app.h"
#include "commands.h"
#include "config.h"
#include "lua_runtime.h"
#include "window.h"

#include <iostream>

using namespace coconut;

int main() {
  // C++ defaults (used when config file is absent).
  Config cfg{};

  // Step 1: load config file (keep defaults on failure).
  // Tries coconut.config.lua first, then coconut.config.json, then C++ defaults.
  auto cfg_result = coconut::loadConfig();
  if (cfg_result) {
    cfg = cfg_result.value();
  } else {
    const auto err = cfg_result.error();
    std::cerr << "Config load failed (keeping defaults): " << err.message
              << " (" << err.details << ")\n";
    std::cerr << "  → Place coconut.config.lua (or coconut.config.json) in the working directory.\n";
  }

  std::cerr << "[debug] main: creating app...\n";
  // Step 2: create app (App core owns WebUI window id + context).
  auto app_result = coconut::app::create(&cfg);
  if (!app_result) {
    std::cerr << "Failed to create app: " << app_result.error().message
              << "\n";
    return 1;
  }
  auto* app = app_result.value();

  std::cerr << "[debug] main: creating commands registry...\n";
  // Step 3: create command registry (needed before ctx:bind is called).
  {
    auto cmd_result = coconut::commands::create(&cfg);
    if (!cmd_result) {
      std::cerr << "Failed to create commands registry: "
                << cmd_result.error().message << "\n";
      coconut::app::destroy(app);
      return 1;
    }
    app->commands = cmd_result.value();
    app->context->commands = app->commands;
  }

  std::cerr << "[debug] main: creating window...\n";
  // Step 4: create window wrapper using the app-owned WebUI window id.
  auto window_result = coconut::window::createWindow(&cfg, app->window_id);
  if (!window_result) {
    std::cerr << "Failed to create window: "
              << window_result.error().message << "\n";
    coconut::app::destroy(app);
    return 1;
  }
  auto* window = window_result.value();
  app->window = window;
  app->context->window = window;

  std::cerr << "[debug] main: creating lua runtime...\n";
  // Step 5: create Lua runtime.
  auto lua_result = coconut::lua::create(&cfg, app->context);
  if (!lua_result) {
    std::cerr << "Failed to create Lua runtime: " << lua_result.error().message
              << "\n";
    coconut::app::destroy(app);
    return 1;
  }
  auto* lua_runtime = lua_result.value();
  app->lua_state = lua_runtime;

  // Wire back: runtime needs app for bridge access.
  lua_runtime->app = app;

  // Step 6: load user entry point (main.lua) and apply coconut.config(ctx).
  // The loadEntryPoint function handles:
  //   • loading main.lua
  //   • calling coconut.config(ctx) if it exists
  //   • merging returned config fields (table) into cfg
  // The ctx setters (setBrowser, setWindowSize, setInitialView) mutate the
  // shared Config in-place — merging app-level overrides on top of the
  // config-file defaults.
  std::cerr << "[debug] main: loading entry point...\n";
  auto entry_result = coconut::lua::loadEntryPoint(lua_runtime, &cfg);
  if (!entry_result && entry_result.error().code != ErrorCode::Ok) {
    // Log the error but continue — non-fatal; app runs with current config.
    std::cerr << "[warn] entry-point: " << entry_result.error().message
              << " (" << entry_result.error().details << ")\n";
  }

  std::cerr << "[debug] main: calling app::run()...\n";
  coconut::app::run(app);

  coconut::app::destroy(app);
  return 0;
}
