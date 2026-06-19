#include "dispatch.h"
#include "app.h"
#include "debug.h"

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
  // This will call dispatch::drain() on each event loop iteration.
}

void shutdown(App* app) {
  // Drain any remaining messages before shutting down.
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
        debug::info(std::format("dispatch::drain: EvalJS ({})",
                                 msg->payload.substr(0, 100)));
        // TODO: Call webview_eval(app->webview, msg->payload.c_str())
        // once the webview is guaranteed to be in a safe state.
        break;

      case MessageKind::LifecycleEvent:
        debug::info(std::format("dispatch::drain: LifecycleEvent ({})",
                                 msg->payload));
        // TODO: Call lua::dispatchViewLifecycleEvent(runtime, viewName, eventName)
        // once the Lua state is guaranteed to be initialized.
        break;

      case MessageKind::CommandCall:
        debug::info(std::format("dispatch::drain: CommandCall ({})",
                                 msg->payload));
        // TODO: Dispatch to the command registry.
        break;
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
