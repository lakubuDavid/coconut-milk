#include "dispatch.h"

namespace coconut::dispatch {

// ── Outbox — lock-free SPSC ring buffer ──────────────────────────────

bool Outbox::push(Message msg) {
  // Snapshot both indices atomically.
  const size_t w = write_idx_.load(std::memory_order_relaxed);
  const size_t r = read_idx_.load(std::memory_order_acquire);

  // Queue is full when the producer has wrapped entirely around.
  if (w - r >= kQueueCapacity) {
    return false;
  }

  ring_[w % kQueueCapacity] = std::move(msg);

  // Ensure the message is fully written before publishing w+1 to the
  // consumer thread.
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

  // Ensure the message is fully read before publishing r+1 (which tells
  // the producer that this slot is available for reuse).
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
  // TODO: Register CFRunLoopSource on main thread.
  // This will be wired up once the App struct holds an Outbox.
}

void shutdown(App* app) {
  (void)app;
  // TODO: Remove CFRunLoopSource, drain remaining messages.
}

void drain(App* app) {
  (void)app;
  // TODO: Pop messages from the App's Outbox and dispatch them.
  // This will be called by the CFRunLoopSource on each iteration.
}

// ── Enqueue helpers ───────────────────────────────────────────────────

void evalJS(App* app, std::string_view js) {
  (void)app;
  (void)js;
  // TODO: Push an EvalJS message into the App's Outbox.
}

void lifecycleEvent(App* app, std::string_view view_name,
                    std::string_view event_name) {
  (void)app;
  (void)view_name;
  (void)event_name;
  // TODO: Push a LifecycleEvent message into the App's Outbox.
}

void commandCall(App* app, std::string_view command_name,
                 std::string_view json_args) {
  (void)app;
  (void)command_name;
  (void)json_args;
  // TODO: Push a CommandCall message into the App's Outbox.
}

}  // namespace coconut::dispatch
