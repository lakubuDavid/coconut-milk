#include "app.h"
#include "commands.h"
#include "config.h"
#include "debug.h"
#include "lifecycle.h"
#include "lua_runtime.h"
#include "window.h"

// Custom URL scheme handler for coconut:// assets.
#include "platform/scheme_handler.h"

#include <filesystem>
#include <iostream>
#include <memory>
#include <print>
#include <vector>

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
    debug::warn(std::format("Config load failed (keeping defaults): {} ({})",
                            err.message, err.details));
    debug::info("Place coconut.config.lua (or coconut.config.json) in the working directory.");
  }

  // Install custom URL scheme handler (coconut://) before webview_create().
  // On macOS this sets the pre-WKWebView-configuration hook.
  // On other platforms this stores the root dir for later registration.
  debug::info("main: installing coconut:// scheme handler...");
  auto app_root = std::filesystem::absolute(".").string();
  platform::installSchemeHandlerHook(app_root);

  debug::info("main: creating app...");
  // Step 2: create app (App core owns WebUI window id + context).
  auto app_result = coconut::app::create(&cfg);
  if (!app_result) {
    debug::error(std::format("Failed to create app: {}", app_result.error().message));
    return 1;
  }
  auto* app = app_result.value();

  debug::info("main: creating commands registry...");
  // Step 3: create command registry (needed before ctx:bind is called).
  {
    auto cmd_result = coconut::commands::create(&cfg);
    if (!cmd_result) {
      debug::error(std::format("Failed to create commands registry: {}",
                                cmd_result.error().message));
      coconut::app::destroy(app);
      return 1;
    }
    app->commands = cmd_result.value();
    app->context->commands = app->commands;
  }

  debug::info("main: creating bridge state...");
  // Step 3b: create bridge state (needed before transport creation).
  {
    auto bridge_result = coconut::bridge::create(&cfg);
    if (!bridge_result) {
      debug::error(std::format("Failed to create bridge state: {}",
                                bridge_result.error().message));
      coconut::app::destroy(app);
      return 1;
    }
    app->bridge_state = bridge_result.value();
  }

  debug::info("main: creating window...");
  // Step 4: create window wrapper using the app-owned webview handle.
  auto window_result = coconut::window::createWindow(&cfg, app->webview);
  if (!window_result) {
    debug::error(std::format("Failed to create window: {}",
                              window_result.error().message));
    coconut::app::destroy(app);
    return 1;
  }
  auto* window = window_result.value();
  app->window = window;
  app->context->window = window;

  // Apply native window style (frameless, etc.) before showing any view.
  window::applyWindowStyle(window);

  debug::info("main: creating lua runtime...");
  // Step 5: create Lua runtime.
  auto lua_result = coconut::lua::create(&cfg, app->context);
  if (!lua_result) {
    debug::error(std::format("Failed to create Lua runtime: {}",
                              lua_result.error().message));
    coconut::app::destroy(app);
    return 1;
  }
  auto* lua_runtime = lua_result.value();
  app->lua_state = lua_runtime;

  // Wire back: runtime needs app for bridge access.
  lua_runtime->app = app;

  // Wire ctx.window Lua binding now that the app pointer is available.
  coconut::lua::wireWindowHandle(lua_runtime);

  // Create the bridge transport and bind JS entry points.
  // Must happen after runtime->app is set (transport needs the App*).
  bridge::createTransport(app);

  // Finalize the coconut:// scheme handler after the transport / webview
  // is fully initialized.  On macOS this is a no-op (done via pre-webview
  // hook).  On Windows/Linux this registers the handler now.
  debug::info("main: finalizing coconut:// scheme handler...");
  platform::finalizeSchemeHandler(app->webview);

  // Register Cocoa NSWindow lifecycle observers (resize, focus, blur).
  // These emit bridge events so the frontend can listen with coconut.on().
  lifecycle::registerEvents(app);

  // Step 6: load user entry point (main.lua) and apply coconut.config(ctx).
  // The loadEntryPoint function handles:
  //   • loading main.lua
  //   • calling coconut.config(ctx) if it exists
  //   • merging returned config fields (table) into cfg
  // The ctx setters (setBrowser, setWindowSize, setInitialView) mutate the
  // shared Config in-place — merging app-level overrides on top of the
  // config-file defaults.
  debug::info("main: loading entry point...");
  auto entry_result = coconut::lua::loadEntryPoint(lua_runtime, &cfg);
  if (!entry_result && entry_result.error().code != ErrorCode::Ok) {
    // Log the error but continue — non-fatal; app runs with current config.
    debug::warn(std::format("entry-point: {} ({})",
                             entry_result.error().message,
                             entry_result.error().details));
  }

  // Step 7: load view descriptors into the window.
  // Views come from two sources:
  //   - the config file (coconut.config.lua / coconut.config.json) via
  //     `Config::views`
  //   - the Lua entry point's `coconut.views()` which also populates
  //     `Config::views` during loadEntryPoint
  // Convert each ViewEntry into a window::View and register it.
  // Ownership passes to the window — destroyWindow frees the views.
  for (const auto& [name, entry] : cfg.views) {
    window::ViewKind kind;
    if (entry.kind == "file")
      kind = window::VIEW_KIND_FILE;
    else if (entry.kind == "html")
      kind = window::VIEW_KIND_HTML;
    else {
      debug::info(std::format("skipping view '{}': unknown kind '{}'",
                              name, entry.kind));
      continue;
    }

    debug::info(std::format("creating view '{}' ({}, {}...)",
                             name, entry.kind, entry.src.substr(0, 60)));
    auto view_result = window::createView(entry.src, kind, std::nullopt);
    if (!view_result) {
      debug::warn(std::format("failed to create view '{}': {}",
                               name, view_result.error().message));
      continue;
    }

    auto* v = new window::View(std::move(*view_result));
    window::addView(window, name, v);
    debug::info(std::format("view '{}' registered", name));
  }

  for(const auto [k,v] : window->views){
    std::println("Views: {} ",k);
  }

  

  // Show the initial view once all views are registered.
  if (!cfg.initial_view.empty()) {
    // kReady is baked into the webview_init() script (see createTransport).
    // No need to signalReady — it auto-fires when the page loads.
    debug::info(std::format("showing initial view '{}'", cfg.initial_view));
    window::showView(window, cfg.initial_view);
  }

  debug::info("main: calling app::run()...");
  coconut::app::run(app);

  coconut::app::destroy(app);
  return 0;
}
