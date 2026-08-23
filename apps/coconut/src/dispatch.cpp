#include "dispatch.h"
#include "app.h"
#include "bg_runtime.h"
#include "bridge.h"
#include "debug.h"
#include "main_runtime.h"

#include <nlohmann/json.hpp>
#include "common.h"
#include "core/messages.h"  // JsRPCMessage

#include <format>

// ── Platform-specific run-loop integration ─────────────────────────────
// On macOS, a CFRunLoopSource is registered to drain the outbox on every
// iteration of the main run loop.  Linux would use a GMainLoop idle
// callback; Windows would use a timer or a custom Windows message.
//
// The source fires at the "common modes" priority so it also fires during
// modal tracking (menus, scroll views) — important for timely dispatch of
// command results and view lifecycle events.

#if defined(__APPLE__)
#include <CoreFoundation/CFRunLoop.h>
#endif

namespace {

  /// The App pointer used by the platform run-loop callback.
  /// Set by coconut::dispatch::init(), cleared by shutdown().
  /// Only one App at a time per process.
  coconut::App* g_dispatch_app = nullptr;

#if defined(__APPLE__)
  /// The CFRunLoopSource registered by init() and removed by shutdown().
  CFRunLoopSourceRef g_runloop_source = nullptr;
#endif

}  // anonymous namespace

namespace coconut::dispatch {

  // ── Outbox — lock-free SPSC ring buffer ──────────────────────────────

  bool Outbox::push(Message msg) {
    const size_t w = write_idx_.load(std::memory_order_relaxed);
    const size_t r = read_idx_.load(std::memory_order_acquire);

    if (w - r >= kQueueCapacity) {
      return false;
    }

    ring_[w % kQueueCapacity] = std::move(msg);
    write_idx_.store(w + 1, std::memory_order_release);
    return true;
  }

  std::optional<Message> Outbox::pop() {
    const size_t r = read_idx_.load(std::memory_order_relaxed);
    const size_t w = write_idx_.load(std::memory_order_acquire);

    if (r == w) {
      return std::nullopt;
    }

    Message msg = ring_[r % kQueueCapacity];
    read_idx_.store(r + 1, std::memory_order_release);
    return msg;
  }

  bool Outbox::empty() const {
    return size() == 0;
  }

  size_t Outbox::size() const {
    const size_t w = write_idx_.load(std::memory_order_acquire);
    const size_t r = read_idx_.load(std::memory_order_acquire);
    return w - r;
  }

  // ── Lifecycle ─────────────────────────────────────────────────────────

  void init(App* app) {
    if (app == nullptr) {
      return;
    }
    g_dispatch_app = app;

#if defined(__APPLE__)
    // Create a CFRunLoopSource whose perform callback drains the outbox.
    // The source fires on every iteration of the main run loop.
    CFRunLoopSourceContext ctx{};
    ctx.version = 0;
    ctx.info    = nullptr;  // We use g_dispatch_app instead.
    ctx.perform = [](void* /*info*/) { drain(g_dispatch_app); };

    g_runloop_source = CFRunLoopSourceCreate(kCFAllocatorDefault, 0, &ctx);

    if (g_runloop_source) {
      CFRunLoopAddSource(CFRunLoopGetMain(), g_runloop_source, kCFRunLoopCommonModes);
      debug::info("dispatch::init: CFRunLoopSource registered (common modes)");
    } else {
      debug::warn("dispatch::init: failed to create CFRunLoopSource");
    }
#else
    debug::info("dispatch::init: no platform run-loop integration (polling not yet implemented)");
#endif
  }

  void shutdown(App* app) {
    // Drain any remaining messages before tearing down.
    drain(app);

#if defined(__APPLE__)
    if (g_runloop_source) {
      CFRunLoopRemoveSource(CFRunLoopGetMain(), g_runloop_source, kCFRunLoopCommonModes);
      CFRelease(g_runloop_source);
      g_runloop_source = nullptr;
      debug::info("dispatch::shutdown: CFRunLoopSource removed");
    }
#endif

    g_dispatch_app = nullptr;
    (void)app;
  }

  void drain(App* app) {
    if (app == nullptr) {
      return;
    }

    // Transitional (Phase 1): also flush the core Dispatcher when wired,
    // so queued DispatchMessages route while the legacy path still runs.
    if (app->dispatcher != nullptr) {
      app->dispatcher->flush();
    }

    while (auto msg = app->outbox.pop()) {
      switch (msg->kind) {
        case MessageKind::EvalJS:
          if (app->webview != nullptr) {
            webview_eval(app->webview, msg->payload.c_str());
          }
          break;

        case MessageKind::LifecycleEvent: {
          // Split "viewName|eventName".
          size_t pipe = msg->payload.find('|');
          if (pipe == std::string::npos) {
            debug::warn("dispatch::drain: malformed LifecycleEvent payload");
            break;
          }
          std::string_view view(msg->payload.data(), pipe);
          std::string_view event(msg->payload.data() + pipe + 1, msg->payload.size() - pipe - 1);

          if (app->lua_state != nullptr) {
            lua::dispatchViewLifecycleEvent(
                app->lua_state, std::string(view), std::string(event), {}
            );
          }
          break;
        }

        case MessageKind::CommandCall: {
          // Split "commandName|jsonArgs".
          size_t pipe = msg->payload.find('|');
          if (pipe == std::string::npos) {
            debug::warn("dispatch::drain: malformed CommandCall payload");
            break;
          }
          std::string_view cmd(msg->payload.data(), pipe);
          std::string_view args(msg->payload.data() + pipe + 1, msg->payload.size() - pipe - 1);

          // TODO: Dispatch to command registry via bridge.
          debug::info(std::format("dispatch::drain: CommandCall '{}' (not yet dispatched)", cmd));
          (void)args;
          break;
        }
      }
    }

    // Drain background thread outbox (results from bg commands).
    if (app->bg != nullptr) {
      while (auto msg = app->bg->outbox.pop()) {
        switch (msg->kind) {
          case MessageKind::CommandResult: {
            // Payload: "callId|jsonResult"
            size_t pipe = msg->payload.find('|');
            if (pipe == std::string::npos) {
              debug::warn("dispatch::drain: malformed bg CommandResult payload");
              break;
            }
            std::string callId(msg->payload.data(), pipe);
            std::string resultJson(msg->payload.data() + pipe + 1, msg->payload.size() - pipe - 1);

            coconut::core::JsRPCMessage rpcMsg;
            rpcMsg.id   = callId;
            rpcMsg.type = coconut::core::RpcType::kReturn;
            try {
              rpcMsg.payload = nlohmann::json::parse(resultJson);
            } catch (...) {
              rpcMsg.type    = coconut::core::RpcType::kError;
              rpcMsg.payload = {{"code", "ParseError"}, {"message", "bg result parse error"}};
            }
            bridge::rpcSend(app, rpcMsg);
            break;
          }
          default:
            break;
        }
      }
    }
  }

  void notify(App* app) {
    if (app == nullptr)
      return;
#if defined(__APPLE__)
    if (g_runloop_source) {
      CFRunLoopSourceSignal(g_runloop_source);
      CFRunLoopWakeUp(CFRunLoopGetMain());
    }
#endif
  }

  // ── Enqueue helpers ───────────────────────────────────────────────────

  void evalJS(App* app, std::string_view js) {
    if (app == nullptr) {
      return;
    }
    app->outbox.push({MessageKind::EvalJS, std::string(js)});
  }

  void lifecycleEvent(App* app, std::string_view view_name, std::string_view event_name) {
    if (app == nullptr) {
      return;
    }
    std::string payload = std::string(view_name) + "|" + std::string(event_name);
    app->outbox.push({MessageKind::LifecycleEvent, std::move(payload)});
  }

  void commandCall(App* app, std::string_view command_name, std::string_view json_args) {
    if (app == nullptr) {
      return;
    }
    std::string payload = std::string(command_name) + "|" + std::string(json_args);
    app->outbox.push({MessageKind::CommandCall, std::move(payload)});
  }

}  // namespace coconut::dispatch
