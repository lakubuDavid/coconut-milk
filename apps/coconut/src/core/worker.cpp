#include "worker.h"
#include "core/exec_command.h"
#include "debug.h"
#include "error.h"
#include "modules/bg_stubs.h"
#include "modules/bridge_emit.h"
#include "modules/clipboard.h"
#include "modules/dialog.h"
#include "modules/env.h"
#include "modules/fs.h"
#include "modules/hotreload.h"
#include "modules/json.h"
#include "modules/keybind.h"
#include "modules/log.h"
#include "modules/notify.h"
#include "modules/openurl.h"
#include "modules/registry.h"
#include "modules/store.h"
#include "modules/stubs.h"
#include "modules/thread_kind.h"

#include <atomic>
#include <exception>
#include <format>
#include <memory>
#include <optional>
#include <sol/state.hpp>
#include <sol/types.hpp>
#include <thread>
#include <variant>

using namespace std::chrono_literals;

namespace coconut::core {

std::expected<WorkerPtr, Error> createWorker() {
  WorkerPtr worker(new Worker{}, WorkerDeleter{});

  worker->LuaState = std::make_unique<sol::state>();
  worker->LuaState->open_libraries(sol::lib::string, sol::lib::base,
                                   sol::lib::table, sol::lib::package,
                                   sol::lib::os);

  worker->Input = std::make_unique<MessageQueue<WorkerInput>>();
  worker->Output = std::make_shared<MessageQueue<WorkerOutput>>();
  debug::info("worker created");
  return worker;
}

void WorkerDeleter::operator()(Worker *worker) const noexcept {

  if (!worker) {
    return;
  }

  worker->StopRequested.store(true, std::memory_order_release);

  if (worker->Input) {
    worker->Input->stop();
  }
  if (worker->Output) {
    worker->Output->stop();
  }

  if (worker->Thread.joinable()) {
    worker->Thread.join();
  }

  worker->Commands.clear();
  worker->App.reset();
  worker->LuaState.reset();

  debug::info("worker destroyed");
  delete worker;
}

// std::optional<Error>
// destroyWorker(WorkerPtr worker) {
//   if (!worker) {
//     return std::nullopt;
//   }

//   worker->StopRequested.store(true, std::memory_order_release);

//   if (worker->MessageBox) {
//     worker->MessageBox->Input.stop();
//     worker->MessageBox->Output.stop();
//   }

//   if (worker->Thread.joinable()) {
//     worker->Thread.join();
//   }

//   worker->Commands.clear();
//   worker->App.reset();
//   worker->LuaState.reset();

//   return std::nullopt;
// }

std::optional<Error> bindModules(Worker *worker, modules::ModulesFlag modules) {
  if (!worker || !worker->LuaState)
    return Error{.code = ErrorCode::LuaError,
                 .message = "the worker's lua state not ready"};

  sol::state &lua = *worker->LuaState;

  using F = modules::ModulesFlag;

  if (has(modules, F::JSON))
    modules::init_json(lua, modules::ThreadKind::Background);
  if (has(modules, F::LOG))
    modules::init_log(lua, modules::ThreadKind::Background);
  if (has(modules, F::FS))
    modules::init_fs(lua, modules::ThreadKind::Background);
  if (has(modules, F::ENV))
    modules::init_env(lua, modules::ThreadKind::Background);
  if (has(modules, F::STORE))
    modules::init_store(lua, modules::ThreadKind::Background);
  if (has(modules, F::OPENURL))
    modules::init_openurl(lua, modules::ThreadKind::Background);
  if (has(modules, F::KEYBIND))
    modules::init_keybind(lua, modules::ThreadKind::Background);
  if (has(modules, F::DIALOG))
    modules::init_dialog(lua, modules::ThreadKind::Background);
  if (has(modules, F::NOTIFY))
    modules::init_notify(lua, modules::ThreadKind::Background);
  if (has(modules, F::CLIPBOARD))
    modules::init_clipboard(lua, modules::ThreadKind::Background);
  if (has(modules, F::HOTRELOAD))
    modules::init_hotreload(lua, modules::ThreadKind::Background);
  if (has(modules, F::BRIDGE_EMIT))
    modules::init_bridge_emit(lua, modules::ThreadKind::Background);
  if (has(modules, F::STUBS))
    modules::init_stubs(lua, modules::ThreadKind::Background);
  if (has(modules, F::BG_STUBS))
    modules::init_bg_stubs(lua, modules::ThreadKind::Background);

  return std::nullopt;
}

std::optional<Error> bindCommands(Worker *worker) {
  sol::state &lua = *worker->LuaState;
  for (auto fn : worker->Commands) {
    lua[fn.first] = fn.second;
  }
  return std::nullopt;
}

std::optional<coconut::Error>
bindLuaContext(Worker *worker, std::function<void(sol::state *)> callback) {
  try {
    callback(worker->LuaState.get());
  } catch (std::exception ex) {
  }
}

void Worker::exec(RequestId id, std::string command, nlohmann::json args) {
  if (!Input)
    return;
  Input->push(PromiseMessage{
      .id = id,
      .command = std::move(command),
      .args = std::move(args),
  });
}

void Worker::drain(std::function<void(const WorkerOutput &)> callback) {
  if (!Output)
    return;
  while (auto msg = Output->tryPop()) {
    callback(*msg);
  }
}

std::optional<Error> workerLoop(Worker *worker) {
  if (!worker || !worker->Input) {
    return Error{.message = "Invalid worker"};
  }

  worker->_Running.store(true, std::memory_order_release);
  sol::state_view lua(*worker->LuaState);

  while (!worker->StopRequested.load(std::memory_order_acquire)) {
    auto message = worker->Input->waitAndPop();
    if (!message.has_value()) {
      break; // queue stopped
    }

    std::visit(
        [worker, lua](const PromiseMessage &req) {
          CoconutContext *ctx = worker->App ? worker->App->context : nullptr;

          CommandResult result =
              execCommand(lua, worker->Commands, req.command, req.args, ctx);

          if (result.ok) {
            worker->Output->push(
                ResolveMessage{.id = req.id, .result = std::move(result.data)});
          } else {
            // Extract error message from result.data
            std::string errMsg = "Unknown error";
            if (result.data.is_object() && result.data.contains("message")) {
              errMsg = result.data["message"].get<std::string>();
            }
            debug::error(std::format(
                "[woker_loop] : command execution failed : {}", errMsg));
            worker->Output->push(
                RejectMessage{.id = req.id, .error = std::move(errMsg)});
          }
        },
        *message);
  }

  worker->_Running.store(false, std::memory_order_release);
  return std::nullopt;
}

std::optional<Error> attachWorker(Worker *worker) {
  if (worker == nullptr) {
    return Error{.message = "Invalid worker"};
  }

  if (!worker->Input || !worker->LuaState) {
    return Error{.message = "Worker is not initialized"};
  }

  if (worker->Thread.joinable()) {
    return Error{.message = "Worker thread is already running"};
  }

  worker->Thread = std::thread([worker] { workerLoop(worker); });

  return std::nullopt;
}

std::optional<coconut::Error>
shutdownWorker(coconut::core::Worker *worker, Worker::ShutdownFlag flag,
               std::chrono::milliseconds softAbortTimeout) {
  if (!worker || !worker->Input) {
    return coconut::Error{.message = "Invalid worker"};
  }

  // Signal the worker to stop accepting new commands.
  worker->StopRequested.store(true, std::memory_order_release);
  worker->Input->stop();

  auto deadline = std::chrono::steady_clock::now() + softAbortTimeout;

  if (flag == Worker::ShutdownFlag::SoftAbort) {
    // Let the in-flight command finish, but bound the wait so we don't hang
    // forever on a slow or stuck command.
    while (worker->isRunning() && std::chrono::steady_clock::now() < deadline) {
      std::this_thread::yield();
    }
    if (worker->isRunning()) {
      debug::warn(std::format(
          "shutdownWorker(SoftAbort): timeout after {}ms, detaching thread",
          softAbortTimeout.count()));
      if (worker->Thread.joinable()) {
        worker->Thread.detach();
      }
    }
  } else {
    // HardAbort: use 10ms grace period, then detach immediately.
    auto hardDeadline = std::chrono::steady_clock::now() + 10ms;
    while (worker->isRunning() &&
           std::chrono::steady_clock::now() < hardDeadline) {
      std::this_thread::yield();
    }
    if (worker->isRunning() && worker->Thread.joinable()) {
      debug::warn("shutdownWorker(HardAbort): detaching stuck thread");
      worker->Thread.detach();
    }
  }

  // Drain final results before cleanup.
  worker->drain([](const auto &out) { /* handle */ });

  return std::nullopt;
}

std::expected<std::unique_ptr<WorkerPool>, coconut::Error>
createWorkerPool(int size) {
  if (size <= 0) {
    return std::unexpected(
        Error{.message = "createWorkerPool: size must be > 0"});
  }

  auto pool = std::make_unique<WorkerPool>();
  pool->Output = std::make_shared<MessageQueue<WorkerOutput>>();

  for (int i = 0; i < size; ++i) {
    auto w = createWorker();
    if (!w) {
      return std::unexpected(w.error());
    }
    pool->Workers.push_back(std::move(w.value()));
  }

  debug::info("createWorkerPool: created " + std::to_string(size) + " workers");
  return pool;
}

std::expected<std::unique_ptr<WorkerPool>, coconut::Error>
createWorkerPool(int size, WorkerInitializer init) {
  auto pool = createWorkerPool(size);
  if (!pool) {
    return std::unexpected(pool.error());
  }
  auto poolPtr = std::move(pool.value());
  for (auto &w : poolPtr->Workers) {
    auto e = init(w.get());
    if (e) {
      // Partial workers are cleaned up when poolPtr is destroyed.
      return std::unexpected(*e);
    }
  }
  return poolPtr;
}

WorkerPoolBuilder WorkerPool::builder(int size) {
  return WorkerPoolBuilder{.Size = size};
}

WorkerPoolBuilder &WorkerPoolBuilder::withModules(
    coconut::modules::ModulesFlag modules) {
  Steps.push_back([modules](Worker *w) -> std::optional<Error> {
    return bindModules(w, modules);
  });
  return *this;
}

WorkerPoolBuilder &WorkerPoolBuilder::withCommands(WorkerInitializer loader) {
  Steps.push_back(std::move(loader));
  return *this;
}

WorkerPoolBuilder &WorkerPoolBuilder::withInitializer(WorkerInitializer init) {
  Steps.push_back(std::move(init));
  return *this;
}

std::expected<std::unique_ptr<WorkerPool>, coconut::Error>
WorkerPoolBuilder::build() {
  // Compose all steps into one initializer that runs them in order per worker.
  auto composed = [steps = Steps](Worker *w) -> std::optional<Error> {
    for (auto &step : steps) {
      auto e = step(w);
      if (e) {
        return e;
      }
    }
    return std::nullopt;
  };
  return createWorkerPool(Size, composed);
}

std::optional<coconut::Error> WorkerPool::attachAll() {
  if (Workers.empty()) {
    return coconut::Error{.message = "WorkerPool: no workers to attach"};
  }
  for (auto &w : Workers) {
    if (!w) {
      return coconut::Error{.message = "WorkerPool: null worker"};
    }
    // Share the pool's output queue across all workers (fan-in).
    w->Output = Output;
    auto e = attachWorker(w.get());
    if (e) {
      return e;
    }
  }
  return std::nullopt;
}

std::optional<coconut::Error> WorkerPool::queueMessage(
    const std::string &command, nlohmann::json args) {
  if (Workers.empty()) {
    return coconut::Error{.message = "WorkerPool: empty pool"};
  }
  // Round-robin to the next worker.
  size_t i = _next.fetch_add(1) % Workers.size();
  RequestId id = _nextId.fetch_add(1);
  Workers[i]->exec(id, command, std::move(args));
  return std::nullopt;
}

std::optional<coconut::Error> WorkerPool::shutdownAll(
    Worker::ShutdownFlag flag) {
  for (auto &w : Workers) {
    if (w) {
      shutdownWorker(w.get(), flag);
    }
  }
  return std::nullopt;
}

} // namespace coconut::core
