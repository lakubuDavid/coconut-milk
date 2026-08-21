#include "../../../src/core/worker.h"
#include "debug.h"
#include "message_queue.h"
#include <chrono>
#include <filesystem>
#include <memory>
#include <optional>
#include <print>
#include <thread>
#include <unordered_map>

using namespace std::chrono_literals;
namespace fs = std::filesystem;

static std::string commandsDir() {
  fs::path src = __FILE__;
  if (src.is_relative()) {
    fs::path cwd = fs::current_path();
    for (fs::path p = cwd; p != p.root_path(); p = p.parent_path()) {
      fs::path candidate = p / "tests/modules/multi_workers/commands";
      if (fs::is_directory(candidate))
        return candidate.string();
    }
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

  lua.script("package.path = package.path .. ';" + dir + "/?.lua'");

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
    if (path.extension() != ".lua") continue;
    auto stem = path.stem().string();
    if (stem.size() < 2 || stem.substr(stem.size() - 2) != ".g") continue;

    auto loadResult = lua.script_file(path.string(), sol::script_pass_on_error);
    if (!loadResult.valid()) continue;

    sol::object ret = loadResult;
    if (!ret.is<sol::function>()) continue;

    auto bindResult = ret.as<sol::function>()(ctx);
    if (!bindResult.valid()) continue;
    ++loaded;
  }

  for (auto &[name, fn] : handlers) {
    worker->Commands[name] = std::move(fn);
  }

  if (loaded == 0) {
    return coconut::Error{.message = "no .g.lua files loaded from " + dir};
  }
  coconut::debug::info("loadCommands: " + std::to_string(loaded) +
                       " modules loaded, " +
                       std::to_string(worker->Commands.size()) +
                       " commands registered");
  return std::nullopt;
}

/// Poll the shared output queue until a ResolveMessage with the given id
/// arrives. Returns the result JSON (or nullopt on reject/timeout).
std::optional<nlohmann::json> waitForResult(
    std::shared_ptr<coconut::core::MessageQueue<coconut::core::WorkerOutput>> q,
    coconut::core::RequestId id,
    std::chrono::milliseconds timeout = 5s) {

  auto deadline = std::chrono::steady_clock::now() + timeout;
  while (std::chrono::steady_clock::now() < deadline) {
    auto msg = q->tryPop();
    if (!msg) {
      std::this_thread::sleep_for(20ms);
      continue;
    }
    std::visit(
        [&](auto &m) {
          if (m.id != id) {
            // Not ours — put it back? For this test we just ignore.
            std::println("  [stray msg id={}]", m.id);
          }
        },
        *msg);

    if (const auto *resolve =
            std::get_if<coconut::core::ResolveMessage>(&*msg)) {
      if (resolve->id == id) return resolve->result;
    } else if (const auto *reject =
                   std::get_if<coconut::core::RejectMessage>(&*msg)) {
      if (reject->id == id) return std::nullopt;
    }
  }
  return std::nullopt;
}

int main() {
  // Worker 1
  auto w1Result = coconut::core::createWorker();
  if (!w1Result.value()) {
    coconut::debug::error("can't create worker 1");
    return 1;
  }
  auto w1 = std::move(w1Result.value());
  loadCommands(w1.get());
  coconut::core::bindCommands(w1.get());

  // Worker 2
  auto w2Result = coconut::core::createWorker();
  if (!w2Result.value()) {
    coconut::debug::error("can't create worker 2");
    return 1;
  }
  auto w2 = std::move(w2Result.value());
  loadCommands(w2.get());
  coconut::core::bindCommands(w2.get());

  // Shared output queue — both workers push here.
  auto sharedMessageQueue =
      std::make_shared<coconut::core::MessageQueue<coconut::core::WorkerOutput>>();

  w1->Output = sharedMessageQueue;
  w2->Output = sharedMessageQueue;

  coconut::core::attachWorker(w1.get());
  coconut::core::attachWorker(w2.get());

  std::println("multi_workers: 2 workers attached, commands loaded\n");

  // ── Test 1: Worker1 echoes, main grabs, pushes to Worker2 ──────────
  std::println("=== Test 1: echo (W1) → main → add (W2) ===");
  w1->exec(1, "echo", {{"message", "hello pipeline"}});

  auto r1 = waitForResult(sharedMessageQueue, 1);
  if (r1 && r1->contains("echoed")) {
    std::string echoed = (*r1)["echoed"].get<std::string>();
    std::println("  W1 echoed: '{}'", echoed);

    // Transform and push to Worker2.
    w2->exec(2, "add", {{"a", 10}, {"b", 32}});
    auto r2 = waitForResult(sharedMessageQueue, 2);
    if (r2 && r2->contains("result")) {
      int result = (*r2)["result"].get<int>();
      std::println("  W2 add(10,32) = {}", result);
    } else {
      std::println("  ERROR: W2 add result missing");
    }
  } else {
    std::println("  ERROR: W1 echo result missing");
  }

  // ── Test 2: Pipeline — W1 computes, main chains to W2 ─────────────
  std::println("\n=== Test 2: pipeline W1.add(3,4)=7 → W2.multiply(7,6)=42 ===");
  w1->exec(3, "add", {{"a", 3}, {"b", 4}});
  auto p1 = waitForResult(sharedMessageQueue, 3);
  if (p1 && p1->contains("result")) {
    int seven = (*p1)["result"].get<int>();
    std::println("  W1 add(3,4) = {}", seven);

    // Feed W1's output into W2.
    w2->exec(4, "multiply", {{"a", seven}, {"b", 6}});
    auto p2 = waitForResult(sharedMessageQueue, 4);
    if (p2 && p2->contains("result")) {
      int fortyTwo = (*p2)["result"].get<int>();
      std::println("  W2 multiply({},6) = {}", seven, fortyTwo);
      const char* status = (fortyTwo == 42) ? "  PASS ✓" : "  FAIL ✗ (expected 42)";
      std::println("{}", status);
    } else {
      std::println("  ERROR: W2 multiply result missing");
    }
  } else {
    std::println("  ERROR: W1 add result missing");
  }

  // ── Test 3: Concurrent — both workers run at once, shared queue ─────
  std::println("\n=== Test 3: concurrent W1+W2 on shared queue ===");
  w1->exec(10, "sleep_ms", {{"ms", 300}});
  w2->exec(11, "sleep_ms", {{"ms", 300}});

  int got = 0;
  auto t0 = std::chrono::steady_clock::now();
  while (got < 2 && (std::chrono::steady_clock::now() - t0) < 5s) {
    auto msg = sharedMessageQueue->tryPop();
    if (msg) {
      if (std::get_if<coconut::core::ResolveMessage>(&*msg)) {
        ++got;
        std::println("  got resolve (id={})",
                     std::get<coconut::core::ResolveMessage>(*msg).id);
      }
    } else {
      std::this_thread::sleep_for(20ms);
    }
  }
  if (got == 2) {
    std::println("  PASS ✓ (both workers reported)");
  } else {
    std::println("  FAIL ✗ (only {} workers reported)", got);
  }

  // ── Test 4: WorkerPool (round-robin + shared output) ───────────────
  std::println("\n=== Test 4: WorkerPool round-robin + shared output ===");
  // Fluent builder: bind commands + register them on each worker.
  auto initWorker = [](coconut::core::Worker *w) -> std::optional<coconut::Error> {
    auto e = loadCommands(w);
    if (e)
      return e;
    return coconut::core::bindCommands(w);
  };
  auto poolResult = coconut::core::WorkerPool::builder(2)
                      .withCommands(initWorker)
                      .build();
  if (!poolResult) {
    std::println("  ERROR: createWorkerPool failed: {}",
                 poolResult.error().message);
  } else {
    auto &pool = poolResult.value();
    pool->attachAll();

    // Round-robin 4 commands across the 2 workers.
    pool->queueMessage("add", {{ "a", 1 }, { "b", 1 }});
    pool->queueMessage("add", {{ "a", 2 }, { "b", 2 }});
    pool->queueMessage("add", {{ "a", 3 }, { "b", 3 }});
    pool->queueMessage("add", {{ "a", 4 }, { "b", 4 }});

    int got = 0;
    auto t0 = std::chrono::steady_clock::now();
    while (got < 4 && (std::chrono::steady_clock::now() - t0) < 5s) {
      auto msg = pool->Output->tryPop();
      if (msg) {
        if (const auto *r = std::get_if<coconut::core::ResolveMessage>(&*msg)) {
          ++got;
          std::println("  [pool resolve id={}] result={}",
                       r->id, r->result.dump());
        }
      } else {
        std::this_thread::sleep_for(20ms);
      }
    }
    if (got == 4) {
      std::println("  PASS ✓ (all 4 pooled commands resolved)");
    } else {
      std::println("  FAIL ✗ (only {} resolved)", got);
    }

    pool->shutdownAll();
  }

  coconut::core::shutdownWorker(w1.get());
  coconut::core::shutdownWorker(w2.get());

  std::println("\ndone");
  return 0;
}
