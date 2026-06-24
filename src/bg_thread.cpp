#include "bg_thread.h"

#include "app.h"
#include "bridge.h"
#include "commands.h"
#include "debug.h"
#include "lua_runtime.h"

#include <sol/state.hpp>

#include <filesystem>
#include <format>
#include <chrono>
#include <thread>

namespace coconut::bg_thread {

// ── Lifecycle ─────────────────────────────────────────────────────────

std::expected<Context*, Error> create(App* app, Config* config) {
  auto bg = new Context();
  bg->app = app;

  bg->lua_state = new sol::state();
  bg->lua_state->open_libraries(
      sol::lib::jit,
      sol::lib::base, sol::lib::package, sol::lib::io, sol::lib::os,
      sol::lib::table, sol::lib::string, sol::lib::math);

  // Create the background command registry.
  auto cmdResult = commands::create(config);
  if (!cmdResult) {
    delete bg->lua_state;
    delete bg;
    return std::unexpected(cmdResult.error());
  }
  bg->commands = cmdResult.value();

  // Create the background CoconutContext.
  // It shares the same Config but has its own command registry.
  auto ctxResult = context::create(config);
  if (!ctxResult) {
    commands::destroy(bg->commands);
    delete bg->lua_state;
    delete bg;
    return std::unexpected(ctxResult.error());
  }
  bg->ctx = ctxResult.value();
  bg->ctx->commands = bg->commands;
  bg->ctx->app = app;
  bg->ctx->lua_state = app->lua_state;  // For ctx:emit() — forwarded to main thread

  return bg;
}

void destroy(Context* bg) {
  if (bg == nullptr) return;
  context::destroy(bg->ctx);
  commands::destroy(bg->commands);
  delete bg->lua_state;
  delete bg;
}

// ── Background Lua state initialization ──────────────────────────────

static void loadBackgroundCommands(App* app, Context* bg) {
  std::string cmdRoot = app->configs ? app->configs->command_root : "commands";
  std::string genDir  = "generated";

  // Add directories to package.path so require() works.
  std::string pkgPath = ";"
      + cmdRoot + "/?.lua;"
      + cmdRoot + "/?/init.lua;"
      + genDir  + "/?.lua;"
      + genDir  + "/?/init.lua";
  bg->lua_state->script("package.path = package.path .. '" + pkgPath + "'");

  // Expose the background ctx as a global so .g.lua can call ctx:bind().
  bg->lua_state->set("ctx", bg->ctx);

  int loaded = 0;
  std::vector<std::string> dirsToScan = {cmdRoot, genDir};

  for (const auto& scanDir : dirsToScan) {
    if (!std::filesystem::is_directory(scanDir)) continue;

    for (auto& entry : std::filesystem::directory_iterator(scanDir)) {
      auto path = entry.path();
      if (path.extension() != ".lua") continue;
      auto stem = path.stem().string();

      // Only load .g.lua files (NOT .g_mt.lua — those are for main thread).
      if (stem.size() < 2 || stem.substr(stem.size() - 2) != ".g")
        continue;
      // Skip .g_mt.lua files — they belong to the main thread.
      if (stem.size() >= 5 && stem.substr(stem.size() - 5) == ".g_mt")
        continue;

      std::string cmdName = stem.substr(0, stem.size() - 2);
      debug::info(std::format("[bg] found {}.g.lua, loading...", cmdName));

      auto loadResult = bg->lua_state->script_file(
          path.string(), sol::script_pass_on_error);
      if (!loadResult.valid()) {
        sol::error e = loadResult;
        debug::warn(std::format("[bg] failed to load {}: {}",
                                path.filename().string(), e.what()));
        continue;
      }

      sol::object ret = loadResult;
      if (!ret.is<sol::function>()) {
        debug::warn(std::format("[bg] {} did not return a function (returned type {})",
                                path.filename().string(),
                                static_cast<int>(ret.get_type())));
        continue;
      }

      auto bindResult = ret.as<sol::function>()(bg->ctx);
      if (!bindResult.valid()) {
        sol::error e = bindResult;
        debug::warn(std::format("[bg] register({}) failed: {}", cmdName, e.what()));
      } else {
        ++loaded;
        debug::info(std::format("[bg] registered {} commands", cmdName));
      }
    }
  }

  if (loaded > 0) {
    debug::info(std::format("[bg] loaded {} command module(s)", loaded));
  }
}

// ── Thread run loop ───────────────────────────────────────────────────

static void runLoop(App* app, Context* bg) {
  debug::info("background thread entered run loop");

  while (bg->running.load(std::memory_order_acquire)) {
    bool didWork = false;

    // Process all available messages in the inbox.
    while (auto msg = bg->inbox.pop()) {
      didWork = true;

      switch (msg->kind) {
        case dispatch::MessageKind::CommandCall: {
          // Payload: "commandName|jsonArgs|callId"
          size_t pipe1 = msg->payload.find('|');
          if (pipe1 == std::string::npos) {
            debug::warn("[bg] malformed CommandCall (no first pipe)");
            continue;
          }
          size_t pipe2 = msg->payload.find('|', pipe1 + 1);
          if (pipe2 == std::string::npos) {
            debug::warn("[bg] malformed CommandCall (no second pipe)");
            continue;
          }

          std::string cmdName(msg->payload.data(), pipe1);
          std::string jsonArgs(msg->payload.data() + pipe1 + 1,
                               pipe2 - pipe1 - 1);
          std::string callId(msg->payload.data() + pipe2 + 1,
                             msg->payload.size() - pipe2 - 1);

          // Look up the handler in the background registry.
          auto it = bg->commands->handlers.find(cmdName);
          if (it == bg->commands->handlers.end()) {
            std::string errPayload = callId + "|" +
                R"({"code":"CommandNotFound","message":"No handler for ')" +
                cmdName + "'\"}";
            bg->outbox.push({dispatch::MessageKind::CommandResult,
                             std::move(errPayload)});
            continue;
          }

          // Parse JSON args into a Lua table.
          sol::state_view lua(*bg->lua_state);
          sol::table params = lua.create_table();
          try {
            auto json = nlohmann::json::parse(jsonArgs);
            params = bridge::toTable(lua, json);
          } catch (...) {
            // params stays empty table on parse failure.
          }

          // Invoke the handler.
          auto result = it->second(params, bg->ctx);

          // Serialize result and push back to main thread.
          std::string resultPayload;
          if (result.valid()) {
            sol::object val = result;
            nlohmann::json resultJson;
            if (val.is<sol::table>()) {
              resultJson = bridge::toJson(val.as<sol::table>());
            } else if (val.is<std::string>()) {
              resultJson = val.as<std::string>();
            } else if (val.is<long long>()) {
              resultJson = val.as<long long>();
            } else if (val.is<double>()) {
              resultJson = val.as<double>();
            } else if (val.is<bool>()) {
              resultJson = val.as<bool>();
            } else {
              resultJson = nullptr;
            }
            resultPayload = callId + "|" + resultJson.dump();
          } else {
            sol::error err = result;
            nlohmann::json errJson = {
                {"code", "LuaError"},
                {"message", err.what()}
            };
            resultPayload = callId + "|" + errJson.dump();
          }

          bg->outbox.push({dispatch::MessageKind::CommandResult,
                           std::move(resultPayload)});
          break;
        }

        default:
          debug::warn(std::format("[bg] unknown message kind: {}",
                                  static_cast<int>(msg->kind)));
          break;
      }
    }

    if (!didWork) {
      // No messages — sleep to avoid busy-waiting.
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
  }

  debug::info("background thread stopped");
}

// ── Start / Stop ──────────────────────────────────────────────────────

void start(Context* bg) {
  if (bg == nullptr) return;

  // Load .g.lua files into the background Lua state BEFORE starting the
  // thread. This avoids a race where the main thread sends a command
  // before the background thread has finished registering handlers.
  loadBackgroundCommands(bg->app, bg);

  bg->running.store(true, std::memory_order_release);
  bg->thread = std::thread([bg]() {
    runLoop(bg->app, bg);
  });
}

void stop(Context* bg) {
  if (bg == nullptr) return;
  bg->running.store(false, std::memory_order_release);
  if (bg->thread.joinable()) {
    bg->thread.join();
  }
}

}  // namespace coconut::bg_thread
