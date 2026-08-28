#include "dispatcher.h"
#include "app.h"
#include "debug.h"
#include "main_runtime.h"  // lua::dispatchViewLifecycleEvent
#include "platform/runloop.h"
#include "worker.h"  // WorkerPool — full type for queueMessage/Output

#include <memory>
#include <mutex>
#include <string>
#include <type_traits>
#include <variant>

namespace coconut::core {

  // ── DispatcherBuilder ───────────────────────────────────────────────────

  DispatcherBuilder& DispatcherBuilder::withRuntime(lua::Runtime* runtime) {
    RuntimePtr = runtime;
    return *this;
  }

  DispatcherBuilder& DispatcherBuilder::withWorkerPool(std::unique_ptr<WorkerPool> pool) {
    WorkerPoolPtr = std::move(pool);
    return *this;
  }

  DispatcherBuilder& DispatcherBuilder::withTransport(
      std::shared_ptr<transport::Transport> transport
  ) {
    TransportPtr = std::move(transport);
    return *this;
  }

  std::expected<std::unique_ptr<Dispatcher>, coconut::Error> DispatcherBuilder::build() {
    if (!RuntimePtr) {
      return std::unexpected(
          coconut::Error{
              .code    = ErrorCode::InvalidConfig,
              .message = "DispatcherBuilder::build: runtime is null"
          }
      );
    }
    if (!WorkerPoolPtr) {
      return std::unexpected(
          coconut::Error{
              .code    = ErrorCode::InvalidConfig,
              .message = "DispatcherBuilder::build: worker pool is null"
          }
      );
    }
    if (!TransportPtr) {
      return std::unexpected(
          coconut::Error{
              .code    = ErrorCode::InvalidConfig,
              .message = "DispatcherBuilder::build: transport is null"
          }
      );
    }

    return std::unique_ptr<Dispatcher>(
        new Dispatcher(RuntimePtr, std::move(WorkerPoolPtr), std::move(TransportPtr))
    );
  }

  // ── Dispatcher ──────────────────────────────────────────────────────────

  Dispatcher::Dispatcher(
      lua::Runtime*                         runtime,
      std::unique_ptr<WorkerPool>           pool,
      std::shared_ptr<transport::Transport> transport
  )
      : _Runtime(runtime), _WorkerPool(std::move(pool)), _Transport(std::move(transport)) {
  }

  Dispatcher::~Dispatcher() = default;

  void Dispatcher::queue(DispatchMessageT message) {
    this->_MessageQueue.push(std::move(message));
  }

  void Dispatcher::post(std::function<void()> fn) {
    if (!fn) {
      return;
    }
    {
      std::lock_guard<std::mutex> lock(_TasksMtx);
      _Tasks.push_back(std::move(fn));
    }
    notify();  // wake the main run loop (no-op before init())
  }

  void Dispatcher::notify() {
    platform::runloopNotify();
  }

  void Dispatcher::init() {
    platform::runloopInit([] { /* flush() is invoked by the runloop callback */ });
  }

  void Dispatcher::shutdown() {
    flush();  // drain any remaining messages before tearing down
    platform::runloopShutdown();
  }

  void Dispatcher::flush() {
    //  1. Drain inbound dispatch messages
    while (auto maybe = _MessageQueue.tryPop()) {
      DispatchMessageT msg = std::move(*maybe);
      std::visit(
          [this](auto&& m) {
            using T = std::decay_t<decltype(m)>;

            if constexpr (std::is_same_v<T, CommandCallMessage>) {
              // Route command calls to the worker pool for background execution.
              if (_WorkerPool) {
                auto err = _WorkerPool->queueMessage(m.CommandName, m.Args, m.RpcId);
                if (err) {
                  debug::warn(
                      std::format("Dispatcher: queue '{}' failed: {}", m.CommandName, err->message)
                  );
                }
              } else {
                debug::warn("Dispatcher: no worker pool for command '" + m.CommandName + "'");
              }

            } else if constexpr (std::is_same_v<T, LifecycleMessage>) {
              // Forward view lifecycle events to the Lua runtime (main thread).
              if (_Runtime) {
                lua::dispatchViewLifecycleEvent(_Runtime, m.ViewName, m.EventName, {});
              }

            } else if constexpr (std::is_same_v<T, JsCallMessage>) {
              // Forward the RPC envelope to the Webview via the transport
              // (e.g. a kCall / kEvent). No raw webview_eval.
              if (_Transport) {
                _Transport->send(m.Message);
              }
            }
          },
          msg
      );
    }

    //  2. Drain worker results and route them back to the Webview
    if (_WorkerPool && _WorkerPool->Output) {
      while (auto maybe = _WorkerPool->Output->tryPop()) {
        WorkerOutput out = std::move(*maybe);
        std::visit(
            [this](auto&& o) {
              using T = std::decay_t<decltype(o)>;

              if constexpr (std::is_same_v<T, ResolveMessage>) {
                if (_Transport) {
                  debug::info(
                      std::format(
                          "dispatcher: resolve id='{}' (worker req {})",
                          !o.RpcId.empty() ? o.RpcId : std::to_string(o.id),
                          o.id
                      )
                  );
                  JsRPCMessage rpcMsg;
                  rpcMsg.id   = !o.RpcId.empty() ? o.RpcId : std::to_string(o.id);
                  rpcMsg.type = RpcType::kReturn;
                  // Same envelope shape as the Bridge's sync replies.
                  rpcMsg.payload = {{"ok", true}, {"data", o.result}};
                  _Transport->send(rpcMsg);
                }
              } else if constexpr (std::is_same_v<T, RejectMessage>) {
                if (_Transport) {
                  debug::info(
                      std::format(
                          "dispatcher: reject id='{}' : {}",
                          !o.RpcId.empty() ? o.RpcId : std::to_string(o.id),
                          o.error
                      )
                  );
                  JsRPCMessage rpcMsg;
                  rpcMsg.id   = !o.RpcId.empty() ? o.RpcId : std::to_string(o.id);
                  rpcMsg.type = RpcType::kError;
                  // Same envelope shape as the Bridge's sync replies.
                  rpcMsg.payload = {
                      {"ok", false}, {"error", {{"code", "WorkerError"}, {"message", o.error}}}
                  };
                  _Transport->send(rpcMsg);
                }
              }
            },
            out
        );
      }
    }

    //  3. Drain posted main-thread closures (worker→native marshalling, etc.).
    std::deque<std::function<void()>> local_tasks;
    {
      std::lock_guard<std::mutex> lock(_TasksMtx);
      local_tasks.swap(_Tasks);
    }
    while (!local_tasks.empty()) {
      auto fn = std::move(local_tasks.front());
      local_tasks.pop_front();
      try {
        if (fn) {
          fn();
        }
      } catch (const std::exception& e) {
        debug::error(std::format("dispatcher: posted task threw: {}", e.what()));
      } catch (...) {
        debug::error("dispatcher: posted task threw (unknown)");
      }
    }
  }

}  // namespace coconut::core

// Free helpers (reach the live dispatcher via a global app pointer)
// Module code that marshals onto the main thread without an App* in scope
// (forwardToMain, store, window) uses these. The pointer is set once during
// App setup via setDispatchApp().

namespace coconut::core {

  static App* g_app = nullptr;

  void setDispatchApp(App* app) {
    g_app = app;
  }

  void dispatchPost(std::function<void()> fn) {
    if (g_app && g_app->dispatcher) {
      g_app->dispatcher->post(std::move(fn));
    } else {
      debug::warn("dispatchPost: no dispatcher available");
    }
  }

  void dispatchNotify() {
    if (g_app && g_app->dispatcher) {
      g_app->dispatcher->notify();
    } else {
      platform::runloopNotify();
    }
  }

}  // namespace coconut::core
