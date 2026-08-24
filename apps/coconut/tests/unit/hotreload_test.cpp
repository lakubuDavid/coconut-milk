#include "hotreload.h"
#include "app.h"
#include "config.h"
#include "test.h"

#include <filesystem>
#include <fstream>
#include <string>

// ── Start / stop are no-ops in v0.1 ──────────────────────────────────

COCONUT_TEST(unit, hotreload_start_stop_noop) {
  coconut::Config cfg{};
  // start/stop should not crash even with null pointers
  coconut::hotreload::start(nullptr, nullptr);
  coconut::hotreload::stop();
  coconut::hotreload::start(nullptr, &cfg);
  coconut::hotreload::stop();
}

COCONUT_TEST(unit, hotreload_drain_pending_noop) {
  coconut::Config cfg{};
  coconut::hotreload::drainPending(nullptr, nullptr);
  coconut::hotreload::drainPending(nullptr, &cfg);
  // Must not crash
}

// ── Trigger with temp files ──────────────────────────────────────────

COCONUT_TEST(unit, hotreload_trigger_no_changes) {
  coconut::Config config{};
  auto            app_result = coconut::app::create(&config);
  if (!app_result)
    return;  // skip: headless CI
  coconut::App* app = app_result.value();

  // Trigger with no command files should be harmless
  coconut::hotreload::trigger(app, &config);

  coconut::app::destroy(app);
}

COCONUT_TEST(unit, hotreload_trigger_with_temp_dir) {
  // Create a temp directory with a test .g.lua file
  auto tmp = std::filesystem::temp_directory_path() / "coconut_hotreload_test";
  std::filesystem::create_directories(tmp);

  // Write a minimal .g.lua file
  {
    std::ofstream f(tmp / "test_cmd.g.lua");
    f << "return function(ctx)\n";
    f << "  ctx:bind('test_hot', function(params)\n";
    f << "    return {ok = true}\n";
    f << "  end)\n";
    f << "end\n";
  }

  coconut::Config config{};
  config.command_root = tmp.string();
  config.view_root    = tmp.string();
  config.asset_root   = tmp.string();

  auto app_result = coconut::app::create(&config);
  if (!app_result)
    return;  // skip: headless CI
  coconut::App* app = app_result.value();

  // Trigger should find and load the file
  coconut::hotreload::trigger(app, &config);

  // Clean up
  coconut::app::destroy(app);
  std::filesystem::remove_all(tmp);
}

COCONUT_TEST(unit, hotreload_trigger_on_changed_file) {
  auto tmp = std::filesystem::temp_directory_path() / "coconut_hotreload_changed";
  std::filesystem::create_directories(tmp);

  // Write initial file
  {
    std::ofstream f(tmp / "greeter.g.lua");
    f << "return function(ctx)\n";
    f << "  ctx:bind('greet', function(params)\n";
    f << "    return {msg = 'hello'}\n";
    f << "  end)\n";
    f << "end\n";
  }

  coconut::Config config{};
  config.command_root = tmp.string();
  config.view_root    = tmp.string();
  config.asset_root   = tmp.string();

  auto app_result = coconut::app::create(&config);
  if (!app_result)
    return;  // skip: headless CI
  coconut::App* app = app_result.value();

  // First load
  coconut::hotreload::trigger(app, &config);

  // Modify the file
  {
    std::ofstream f(tmp / "greeter.g.lua");
    f << "return function(ctx)\n";
    f << "  ctx:bind('greet', function(params)\n";
    f << "    return {msg = 'updated'}\n";
    f << "  end)\n";
    f << "end\n";
  }

  // Second trigger should detect change
  coconut::hotreload::trigger(app, &config);

  coconut::app::destroy(app);
  std::filesystem::remove_all(tmp);
}
