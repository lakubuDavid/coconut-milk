#ifndef CORE_WORKER_H
#define CORE_WORKER_H

#include "app.h"
#include "error.h"
#include "modules/registry.h"
#include <atomic>
#include <expected>
#include <lua.h>
#include <memory>
#include <optional>
#include <sol/forward.hpp>
#include <sol/sol.hpp>
#include <sol/state.hpp>
#include <string_view>
#include <thread>
#include <type_traits>
#include <unordered_map>

using namespace std;

namespace coconut {
namespace core {

using RequestId = std::uint64_t;

struct PromiseMessage {
  RequestId id;
  std::string command;
  std::string argsJson;
};

struct ResolveMessage {
  RequestId id;
  std::string resultJson;
};

struct RejectMessage {
  RequestId id;
  std::string error;
};

using WorkerInput = std::variant<PromiseMessage>;

using WorkerOutput = std::variant<ResolveMessage, RejectMessage>;


/*
The sole purpose of the worker is to run commands,
it needs to know the commands registry,
receives a promise, and queues the outbox response with either a resolve message
or reject messsage.

The promise needs the command name and teh args
The result should iinckude the response (json or oteh object epresentation)
The reject should in clude the error
*/
struct Worker {
  std::unique_ptr<sol::state> LuaState;
  std::unique_ptr<coconut::App> App;
  std::thread Thread;
  std::atomic<bool> StopRequested{false};

  std::unordered_map<std::string, sol::protected_function> Commands;
};

std::expected<std::unique_ptr<Worker>, coconut::Error> createWorker();
std::optional<coconut::Error> destroyWorker(std::unique_ptr<Worker> worker);

/**
@description: Binds teh specificed modules to th worker
*/
std::optional<coconut::Error>
bindModules(Worker *worker, coconut::modules::ModulesFlag modules);
std::optional<coconut::Error> bindCommands(Worker *worker);

std::optional<coconut::Error> attachWorker(Worker *worker);
std::optional<coconut::Error> workerLoop(Worker *worker);
} // namespace core
} // namespace coconut

#endif
