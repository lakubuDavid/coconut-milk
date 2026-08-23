#ifndef CORE_WORKER_H
#define CORE_WORKER_H

#include "error.h"
#include "message_queue.h"

namespace coconut {
  class CoconutContext;  // forward decl — Worker stores only a pointer
}
#include "messages.h"
#include "modules/registry.h"

#include <algorithm>
#include <chrono>
#include <nlohmann/json.hpp>

#include <atomic>
#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <sol/environment.hpp>
#include <sol/forward.hpp>
#include <sol/sol.hpp>
#include <sol/state.hpp>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <variant>
#include <vector>

namespace coconut::core {

  /*
  The sole purpose of the worker is to run commands,
  it needs to know the commands registry,
  receives a promise, and queues the outbox response with either a resolve message
  or reject message.

  The promise needs the command name and the args
  The result should include the response (json or oteh object epresentation)
  The reject should include the error
  */

  struct Worker {
    enum ShutdownFlag { SoftAbort, HardAbort };
    std::atomic<bool>           _Running{false};
    std::unique_ptr<sol::state> LuaState;
    CoconutContext             *Context = nullptr;  ///< command-exec ctx (not owned)
    std::thread                 Thread;
    std::atomic<bool>           StopRequested{false};

    /// Per-worker input queue — exclusive to this worker.
    std::unique_ptr<MessageQueue<WorkerInput>> Input;

    /// Shared output queue — multiple workers can push to the same instance.
    /// The main thread does a single waitAndPop() on the shared queue.
    std::shared_ptr<MessageQueue<WorkerOutput>> Output;

    std::unordered_map<std::string, sol::protected_function> Commands;

    /// Push a command request onto the Input queue (thread-safe, called by main
    /// thread). rpcId echoes through to the Resolve/Reject output messages.
    void exec(RequestId id, std::string command, nlohmann::json args, std::string rpcId = {});

    /// Drain all completed results from the Output queue (called by main thread).
    /// Invokes the callback for each ResolveMessage/RejectMessage.
    void drain(std::function<void(const WorkerOutput &)> callback);

    inline bool isRunning() {
      return this->_Running;
    }
  };

  struct WorkerDeleter {
    void operator()(Worker *worker) const noexcept;
  };

  using WorkerPtr = std::unique_ptr<Worker, WorkerDeleter>;

  std::expected<WorkerPtr, coconut::Error> createWorker();
  // std::optional<coconut::Error> destroyWorker(WorkerPtr worker);

  /**
  @description: Binds the specified modules to thie worker
  */
  std::optional<coconut::Error> bindModules(Worker *worker, coconut::modules::ModulesFlag modules);
  std::optional<coconut::Error> bindCommands(coconut::core::Worker *worker);

  std::optional<coconut::Error> bindLuaContext(std::function<void(sol::state *)> callback);

  std::optional<coconut::Error> attachWorker(coconut::core::Worker *worker);
  std::optional<coconut::Error> shutdownWorker(
      coconut::core::Worker    *worker,
      Worker::ShutdownFlag      flag             = Worker::ShutdownFlag::SoftAbort,
      std::chrono::milliseconds softAbortTimeout = std::chrono::milliseconds{30000}
  );
  std::optional<coconut::Error> workerLoop(coconut::core::Worker *worker);

  /// Called once per worker as it is created inside a pool.
  /// Lets the caller bind modules, commands, a Lua context, the bridge, etc.
  /// Returning an Error aborts pool creation (partial workers are cleaned up).
  using WorkerInitializer = std::function<std::optional<coconut::Error>(Worker *)>;

  /// Fluent builder for WorkerPool. Each `with*` step is applied (in order) to
  /// every worker the pool creates; `build()` composes them via
  /// `createWorker` + `WorkerInitializer`.
  struct WorkerPool;  // forward decl — defined below
  struct WorkerPoolBuilder {
    int                            Size{0};
    std::vector<WorkerInitializer> Steps;

    /// Bind the given Lua modules on every worker.
    WorkerPoolBuilder &withModules(coconut::modules::ModulesFlag modules);
    /// Run a command loader on every worker.
    WorkerPoolBuilder &withCommands(WorkerInitializer loader);
    /// Run an arbitrary initializer on every worker.
    WorkerPoolBuilder &withInitializer(WorkerInitializer init);
    /// Build the pool, applying all steps to each worker.
    std::expected<std::unique_ptr<WorkerPool>, coconut::Error> build();
  };

  struct WorkerPool {
    std::vector<WorkerPtr>                      Workers;
    std::shared_ptr<MessageQueue<WorkerOutput>> Output;
    std::atomic<size_t>                         _next{0};
    std::atomic<RequestId>                      _nextId{1};

    /// Begin a fluent builder for a pool of `size` workers.
    static WorkerPoolBuilder builder(int size);

    /// Attach (start) every worker in the pool, sharing the Output queue.
    std::optional<coconut::Error> attachAll();

    /// Round-robin a command to the next worker. Assigns a unique RequestId.
    /// rpcId (optional) is echoed back on the worker's Resolve/Reject message.
    std::optional<coconut::Error> queueMessage(
        const std::string &command, nlohmann::json args, const std::string &rpcId = {}
    );

    /// Shut down every worker (SoftAbort by default, HardAbort to detach).
    std::optional<coconut::Error> shutdownAll(
        Worker::ShutdownFlag flag = Worker::ShutdownFlag::SoftAbort
    );
  };

  std::expected<WorkerPtr, coconut::Error>                   createWorker();
  std::expected<std::unique_ptr<WorkerPool>, coconut::Error> createWorkerPool(int size);
  std::expected<std::unique_ptr<WorkerPool>, coconut::Error> createWorkerPool(
      int size, WorkerInitializer init
  );
};  // namespace coconut::core

#endif
