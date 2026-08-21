#include "../../../src/commands.h"
#include "../../../src/config.h"
#include "../../../src/context.h"
#include "../../../src/core/worker.h"
#include "debug.h"
#include "error.h"
#include <chrono>
#include <filesystem>
#include <optional>
#include <ostream>
#include <print>
#include <thread>
#include <unordered_map>

using namespace std::chrono_literals;
namespace fs = std::filesystem;

static std::string commandsDir() {
  // Resolve relative to this source file's actual location on disk.
  // __FILE__ is relative to the project root at compile time, but the
  // binary runs from the build directory — so we canonicalize from the
  // project root first.
  fs::path src = __FILE__;
  if (src.is_relative()) {
    // Try project root (xmake.lua directory) as base.
    fs::path cwd = fs::current_path();
    // Walk up until we find tests/modules/workers/commands
    for (fs::path p = cwd; p != p.root_path(); p = p.parent_path()) {
      fs::path candidate = p / "tests/modules/workers/commands";
      if (fs::is_directory(candidate))
        return candidate.string();
    }
    // Fallback: try from the compile-time path relative to cwd.
    fs::path dir = src.parent_path() / "commands";
    if (fs::is_directory(dir))
      return dir.string();
  }
  coconut::debug::warn(
      "commands directory not found (cwd=" + fs::current_path().string() + ")");
  return "";
}

std::optional<coconut::Error> loadCommands(coconut::core::Worker *worker) {
  if (!worker || !worker->LuaState) {
    return coconut::Error{.message = "worker or LuaState is null"};
  }

  sol::state &lua = *worker->LuaState;
  std::string dir = commandsDir();
  if (dir.empty()) {
    return coconut::Error{.message = "commands directory not found"};
  }

  // Extend Lua package.path so .g.lua files can require each other.
  lua.script("package.path = package.path .. ';" + dir + "/?.lua'");

  // Minimal fake ctx table — the .g.lua files only need ctx:bind(name, fn).
  // We can't use CoconutContext here because it's not registered as a
  // sol2 usertype in the worker's Lua state.
  sol::table ctx = lua.create_table();
  std::unordered_map<std::string, sol::protected_function> handlers;
  ctx["bind"] = [&handlers](sol::table, const std::string &name,
                            sol::protected_function fn) {
    handlers[name] = std::move(fn);
  };
  ctx["bind_mt"] = [&handlers](sol::table, const std::string &name,
                               sol::protected_function fn) {
    handlers[name] = std::move(fn);
  };
  ctx["sleep"] = [](int argMs) {
    std::this_thread::sleep_for(std::chrono::milliseconds{argMs});
  };
  lua.set("ctx", ctx);

  int loaded = 0;
  for (const auto &entry : fs::directory_iterator(dir)) {
    auto path = entry.path();
    if (path.extension() != ".lua")
      continue;
    auto stem = path.stem().string();
    // Only load *.g.lua files (convention: command modules).
    if (stem.size() < 2 || stem.substr(stem.size() - 2) != ".g")
      continue;

    coconut::debug::info("loading " + path.filename().string());
    auto loadResult = lua.script_file(path.string(), sol::script_pass_on_error);
    if (!loadResult.valid()) {
      coconut::debug::warn(std::string("script failed: ") + path.string());
      continue;
    }

    sol::object ret = loadResult;
    if (!ret.is<sol::function>()) {
      coconut::debug::warn(std::string("not a function: ") + path.string());
      continue;
    }

    auto bindResult = ret.as<sol::function>()(ctx);
    if (!bindResult.valid()) {
      coconut::debug::warn(std::string("bind call failed: ") + path.string());
      continue;
    }
    coconut::debug::info("loaded " + stem);
    ++loaded;
  }

  // Copy loaded handlers into the worker's command map.
  for (auto &[name, fn] : handlers) {
    worker->Commands[name] = std::move(fn);
  }

  if (loaded == 0) {
    return coconut::Error{.message = "no .g.lua files loaded from " + dir};
  }
  coconut::debug::info(
      "loadCommands: " + std::to_string(loaded) + " modules loaded, " +
      std::to_string(worker->Commands.size()) + " commands registered");
  return std::nullopt;
}

int main() {
  auto workerResult = coconut::core::createWorker();
  if (!workerResult.value()) {
    coconut::debug::error("cant create worker");
    return 1;
  }
  auto worker = std::move(workerResult.value());
  auto loadErr = loadCommands(worker.get());
  if (loadErr) {
    coconut::debug::error("loadCommands: " + loadErr->message);
  }

  coconut::core::bindCommands(worker.get());
  coconut::core::attachWorker(worker.get());

  // std::this_thread::sleep_for(1s);
  // std::println("wait");
  // Test: call the "echo" command loaded from echo.g.lua.
  worker->exec(1, "echo", {{"message", "hello from worker"}});
  std::this_thread::sleep_for(100ms);

  worker->drain([](const coconut::core::WorkerOutput &output) -> void {
    if (const auto *resolve =
            std::get_if<coconut::core::ResolveMessage>(&output)) {
      std::println("[drain:resolved] : {}", resolve->result.dump(2));
    } else if (const auto *reject =
                   std::get_if<coconut::core::RejectMessage>(&output)) {
      std::println("[drain:rejected] : {}", reject->error);
    }
  });

  // std::this_thread::sleep_for(1s);
  // std::println("1");
  // Test: call "add" from math.g.lua.
  worker->exec(2, "add", {{"a", 3}, {"b", 4}});
  worker->exec(3, "add", {{"a", 3}, {"b", 4}});
  std::this_thread::sleep_for(100ms);

  worker->drain([](const coconut::core::WorkerOutput &output) -> void {
    if (const auto *resolve =
            std::get_if<coconut::core::ResolveMessage>(&output)) {
      std::println("[drain:resolved] : {}", resolve->result.dump(2));
    } else if (const auto *reject =
                   std::get_if<coconut::core::RejectMessage>(&output)) {
      std::println("[drain:rejected] : {}", reject->error);
    }
  });

  // Test: long-running task completes normally — verifies ctx.sleep works.
  std::println("\n--- sleep completes normally ---");
  worker->exec(4, "sleep_ms", {{"ms", 500}});
  auto t0 = std::chrono::steady_clock::now();
  // Poll drain every 50ms until we get the result.
  bool gotSleepResult = false;
  while (!gotSleepResult) {
    std::this_thread::sleep_for(50ms);
    worker->drain([&](const coconut::core::WorkerOutput &output) -> void {
      if (const auto *resolve =
              std::get_if<coconut::core::ResolveMessage>(&output)) {
        std::println("[drain:resolved] : {}", resolve->result.dump(2));
        if (resolve->result.contains("delayed_ms") &&
            resolve->result["delayed_ms"] == 500) {
          gotSleepResult = true;
        }
      } else if (const auto *reject =
                     std::get_if<coconut::core::RejectMessage>(&output)) {
        std::println("[drain:rejected] : {}", reject->error);
      }
    });
  }
  auto sleepElapsed = std::chrono::steady_clock::now() - t0;
  std::println("sleep_ms(500) round-trip took {}ms (expected >=500ms)",
               std::chrono::duration_cast<std::chrono::milliseconds>(sleepElapsed)
                   .count());

  // Test: long-running command + SoftAbort with a short timeout.
  // The worker will be sleeping for 2s but the timeout is 500ms — the
  // shutdown should detach the thread instead of blocking.
  std::println("\n--- long-running task + SoftAbort timeout ---");
  worker->exec(5, "sleep_ms", {{"ms", 2000}});
  std::this_thread::sleep_for(50ms);  // let it start

  auto start = std::chrono::steady_clock::now();
  coconut::core::shutdownWorker(
      worker.get(),
      coconut::core::Worker::SoftAbort,
      std::chrono::milliseconds{5000});
  auto elapsed = std::chrono::steady_clock::now() - start;
  std::println("shutdown returned in {}ms",
               std::chrono::duration_cast<std::chrono::milliseconds>(elapsed)
                   .count());

  std::println("done");
  // std::println("2");
}
