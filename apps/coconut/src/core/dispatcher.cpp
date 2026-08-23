#include "dispatcher.h"

#include "debug.h"
#include "main_runtime.h"  // lua::dispatchViewLifecycleEvent
#include "worker.h"        // WorkerPool — full type for queueMessage/Output

#include <memory>
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
      return std::unexpected(coconut::Error{
          .code = ErrorCode::InvalidConfig, .message = "DispatcherBuilder::build: runtime is null"}
      );
    }
    if (!WorkerPoolPtr) {
      return std::unexpected(coconut::Error{
          .code    = ErrorCode::InvalidConfig,
          .message = "DispatcherBuilder::build: worker pool is null"});
    }
    if (!TransportPtr) {
      return std::unexpected(coconut::Error{
          .code    = ErrorCode::InvalidConfig,
          .message = "DispatcherBuilder::build: transport is null"});
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

  void Dispatcher::queue(DispatchMessage message) {
    this->_MessageQueue.push(std::move(message));
  }

  void Dispatcher::flush() {
    //  1. Drain inbound dispatch messages
    while (auto maybe = _MessageQueue.tryPop()) {
      DispatchMessage msg = std::move(*maybe);
      std::visit(
          [this](auto&& m) {
            using T = std::decay_t<decltype(m)>;

            if constexpr (std::is_same_v<T, CommandCallMessage>) {
              // Route command calls to the worker pool for background execution.
              if (_WorkerPool) {
                auto err = _WorkerPool->queueMessage(m.CommandName, m.Args);
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
                  JsRPCMessage rpcMsg;
                  rpcMsg.id      = std::to_string(o.id);
                  rpcMsg.type    = RpcType::kReturn;
                  rpcMsg.payload = o.result;
                  _Transport->send(rpcMsg);
                }
              } else if constexpr (std::is_same_v<T, RejectMessage>) {
                if (_Transport) {
                  JsRPCMessage rpcMsg;
                  rpcMsg.id      = std::to_string(o.id);
                  rpcMsg.type    = RpcType::kError;
                  rpcMsg.payload = {{"code", "WorkerError"}, {"message", o.error}};
                  _Transport->send(rpcMsg);
                }
              }
            },
            out
        );
      }
    }
  }

}  // namespace coconut::core
