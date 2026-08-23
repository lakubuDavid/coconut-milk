#ifndef COCONUT_DISPATCH_H
#define COCONUT_DISPATCH_H

#include "core/messages.h"
#include "error.h"

#include <array>
#include <atomic>
#include <expected>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

// ── Forward declarations ────────────────────────────────────────────
namespace coconut {
  struct App;
}

namespace coconut::dispatch {

  /// Alias consolidated types for backward compatibility.
  using core::Message;
  using core::MessageKind;

  /// Maximum in-flight messages in a single Outbox queue.
  constexpr size_t kQueueCapacity = 64;

  // ─ Lock-free SPSC Outbox ───────────────────────────────────────────
  //
  // Single-producer, single-consumer ring buffer.  The producer (Lua thread
  // or any code path that generates an event) pushes messages.  The consumer
  // (main-thread CFRunLoop source) drains them on each iteration.
  //
  // Thread safety: the producer and consumer sides each touch separate
  // atomic indices, so no lock is needed.  Only one producer and one
  // consumer at a time.

  class Outbox {
   public:
    Outbox() = default;

    // Non-copyable, non-movable (the atomics and ring are tied to this
    // instance by the CFRunLoopSource registration).
    Outbox(const Outbox&)            = delete;
    Outbox& operator=(const Outbox&) = delete;

    /// Push a message into the ring buffer.
    /// Returns false if the queue is full (message dropped).
    bool push(Message msg);

    /// Pop the oldest message, or nullopt if empty.
    std::optional<Message> pop();

    /// Returns true when the queue has no messages.
    bool empty() const;

    /// Number of messages currently in the queue.
    size_t size() const;

   private:
    std::array<Message, kQueueCapacity> ring_{};
    /// Producer index — written only by the producer thread.
    alignas(64) std::atomic<size_t> write_idx_{0};
    /// Consumer index — written only by the consumer thread.
    alignas(64) std::atomic<size_t> read_idx_{0};
  };

  // ── Lifecycle ───────────────────────────────────────────────────────

  /// Create the dispatch system for an App.
  /// Registers a CFRunLoopSource on the main thread that drains the
  /// outbox queue on every iteration.
  ///
  /// Must be called from the main thread after the webview is created.
  void init(App* app);

  /// Signal the run-loop source so pending messages are drained promptly.
  /// Thread-safe — can be called from the background thread.
  void notify(App* app);

  /// Tear down the dispatch system.
  /// Must be called from the main thread during app shutdown.
  void shutdown(App* app);

  /// Drain all queued messages on the current thread.
  /// Called internally by the CFRunLoopSource.  Also exposed so it can
  /// be called synchronously in tests.
  void drain(App* app);

  // ── Enqueue helpers (thread-safe, call from any code path) ──────────

  /// Queue a JavaScript string to be evaluated on the main thread.
  void evalJS(App* app, std::string_view js);

  /// Queue a view lifecycle event ("load", "mount", "unmount").
  void lifecycleEvent(App* app, std::string_view view_name, std::string_view event_name);

  /// Queue a command call to be dispatched in the correct runtime.
  void commandCall(App* app, std::string_view command_name, std::string_view json_args);

}  // namespace coconut::dispatch

#endif  // COCONUT_DISPATCH_H
