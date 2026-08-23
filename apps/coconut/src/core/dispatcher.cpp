#include "dispatcher.h"

#include <webview/webview.h>  // webview_eval
#include "bridge.h"           // bridge::rpcSend — Webview RPC back-channel
#include "debug.h"
#include "main_runtime.h"  // lua::dispatchViewLifecycleEvent
#include "worker.h"        // WorkerPool — full type for queueMessage/Output

#include <string>
#include <type_traits>
#include <variant>

namespace coconut::core {

  Dispatcher::Dispatcher(App* app, WorkerPool* pool) : _App(app), _WorkerPool(pool) {
  }

  void Dispatcher::queue(DispatchMessage message) {
    this->_MessageQueue.push(std::move(message));
  }

  void Dispatcher::flush() {
    // ── 1. Drain inbound dispatch messages ──────────────────────────
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
              if (_App && _App->lua_state) {
                lua::dispatchViewLifecycleEvent(_App->lua_state, m.ViewName, m.EventName, {});
              }

            } else if constexpr (std::is_same_v<T, EvalJSMessage>) {
              // Evaluate JS on the main-thread webview.
              if (_App && _App->webview) {
                webview_eval(_App->webview, m.JsCode.c_str());
              }
            }
          },
          msg
      );
    }

    // ── 2. Drain worker results and route them back to the Webview ─
    if (_WorkerPool && _WorkerPool->Output) {
      while (auto maybe = _WorkerPool->Output->tryPop()) {
        WorkerOutput out = std::move(*maybe);
        std::visit(
            [this](auto&& o) {
              using T = std::decay_t<decltype(o)>;

              if constexpr (std::is_same_v<T, ResolveMessage>) {
                if (_App && _App->webview) {
                  rpc::Message rpcMsg;
                  rpcMsg.id      = std::to_string(o.id);
                  rpcMsg.type    = rpc::Type::kReturn;
                  rpcMsg.payload = o.result;
                  bridge::rpcSend(_App, rpcMsg);
                }
              } else if constexpr (std::is_same_v<T, RejectMessage>) {
                if (_App && _App->webview) {
                  rpc::Message rpcMsg;
                  rpcMsg.id      = std::to_string(o.id);
                  rpcMsg.type    = rpc::Type::kError;
                  rpcMsg.payload = {{"code", "WorkerError"}, {"message", o.error}};
                  bridge::rpcSend(_App, rpcMsg);
                }
              }
            },
            out
        );
      }
    }
  }

}  // namespace coconut::core
