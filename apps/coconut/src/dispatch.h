#ifndef COCONUT_DISPATCH_H
#define COCONUT_DISPATCH_H

#include "core/messages.h"
#include "error.h"

#include <array>
#include <atomic>
#include <deque>
#include <expected>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>

// ── Forward declarations ────────────────────────────────────────────
namespace coconut {
  struct App;
}

namespace coconut::dispatch {

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

  /// Post a closure to run on the MAIN thread during the next drain().
  /// Thread-safe — callable from any thread (workers, timers, etc.).
  /// This is the landing pad for cross-thread native access (window
  /// mutations from worker Lua, dialogs, clipboard, …).
  void post(std::function<void()> fn);

  // ── Enqueue helpers (thread-safe, call from any code path) ──────────

  /// Queue a JavaScript string to be evaluated on the main thread.
  void evalJS(App* app, std::string_view js);

  /// Queue a view lifecycle event ("load", "mount", "unmount").
  void lifecycleEvent(App* app, std::string_view view_name, std::string_view event_name);

  /// Queue a command call to be dispatched in the correct runtime.

}  // namespace coconut::dispatch

#endif  // COCONUT_DISPATCH_H
