#include "dispatch.h"
#include "app.h"
#include "debug.h"
#include "lua_runtime.h"

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
  (void)app;
  debug::info("dispatch::init: dispatch system initialized");
  // TODO: Register CFRunLoopSource on main thread.
}

void shutdown(App* app) {
  drain(app);
  debug::info("dispatch::shutdown: dispatch system shut down");
  // TODO: Remove CFRunLoopSource.
}

void drain(App* app) {
  if (app == nullptr) {
    return;
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
        std::string_view event(msg->payload.data() + pipe + 1,
                               msg->payload.size() - pipe - 1);

        if (app->lua_state != nullptr) {
          lua::dispatchViewLifecycleEvent(
              app->lua_state,
              std::string(view),
              std::string(event),
              {});
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
        std::string_view args(msg->payload.data() + pipe + 1,
                              msg->payload.size() - pipe - 1);

        // TODO: Dispatch to command registry via bridge.
        debug::info(std::format("dispatch::drain: CommandCall '{}' (not yet dispatched)",
                                 cmd));
        (void)args;
        break;
      }
    }
  }
}

// ── Enqueue helpers ───────────────────────────────────────────────────

void evalJS(App* app, std::string_view js) {
  if (app == nullptr) {
    return;
  }
  app->outbox.push({MessageKind::EvalJS, std::string(js)});
}

void lifecycleEvent(App* app, std::string_view view_name,
                    std::string_view event_name) {
  if (app == nullptr) {
    return;
  }
  std::string payload = std::string(view_name) + "|" + std::string(event_name);
  app->outbox.push({MessageKind::LifecycleEvent, std::move(payload)});
}

void commandCall(App* app, std::string_view command_name,
                 std::string_view json_args) {
  if (app == nullptr) {
    return;
  }
  std::string payload = std::string(command_name) + "|" + std::string(json_args);
  app->outbox.push({MessageKind::CommandCall, std::move(payload)});
}

}  // namespace coconut::dispatch
