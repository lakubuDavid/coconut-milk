#ifndef CORE_DISPATCHER_H
#define CORE_DISPATCHER_H

// Forward declarations
namespace coconut {
  struct App;
}

#include <expected>
#include <memory>

#include "error.h"
#include "message_queue.h"
#include "messages.h"
#include "transport.h"  // transport::Transport

namespace coconut::lua {
  struct Runtime;  // forward decl — Dispatcher stores only a pointer
}

namespace coconut::core {

  struct WorkerPool;  // forward decl — defined in worker.h
  class Dispatcher;   // forward decl — defined below

  /// Fluent builder for Dispatcher. Each `with*` step configures a dependency;
  /// `build()` validates and constructs the Dispatcher instance.
  struct DispatcherBuilder {
    lua::Runtime*                         RuntimePtr = nullptr;  ///< borrowed (App-owned)
    std::unique_ptr<WorkerPool>           WorkerPoolPtr;         ///< transferred
    std::shared_ptr<transport::Transport> TransportPtr;          ///< shared with Bridge

    /// Bind the main-thread Lua runtime for lifecycle dispatch. Borrowed —
    /// owned by App, so the Dispatcher must never delete it.
    DispatcherBuilder& withRuntime(lua::Runtime* runtime);
    /// Transfer ownership of the worker pool to the Dispatcher.
    DispatcherBuilder& withWorkerPool(std::unique_ptr<WorkerPool> pool);
    /// Share the webview transport (also used by the Bridge).
    DispatcherBuilder& withTransport(std::shared_ptr<transport::Transport> transport);
    /// Validate configuration and construct the Dispatcher.
    std::expected<std::unique_ptr<Dispatcher>, coconut::Error> build();
  };

  /// Central message hub.
  ///
  /// Receives DispatchMessages (from the Main Runtime / Bridge) and routes
  /// them to the right destination:
  ///   • CommandCallMessage → WorkerPool (background execution)
  ///   • LifecycleMessage   → Main Runtime (Lua: dispatchViewLifecycleEvent)
  ///   • JsCallMessage      → Webview (transport->send, RPC envelope)
  ///
  /// On every flush() it also drains the WorkerPool's shared Output queue
  /// and routes command results back to the Webview via the transport.
  class Dispatcher {
    friend struct DispatcherBuilder;

   private:
    MessageQueue<DispatchMessageT>        _MessageQueue;
    lua::Runtime*                         _Runtime;     ///< borrowed (App-owned)
    std::unique_ptr<WorkerPool>           _WorkerPool;  ///< owned
    std::shared_ptr<transport::Transport> _Transport;   ///< shared with Bridge

    // Main-thread closure queue backing post() — drained in flush().
    std::deque<std::function<void()>> _Tasks;
    std::mutex                        _TasksMtx;

    Dispatcher(
        lua::Runtime*                         runtime,
        std::unique_ptr<WorkerPool>           pool,
        std::shared_ptr<transport::Transport> transport
    );

   public:
    /// Out-of-line so unique_ptr<WorkerPool> can hold a forward-declared type.
    ~Dispatcher();

    void queue(DispatchMessageT message);
    void flush();

    /// Post a closure to run on the MAIN thread during the next flush().
    /// Thread-safe — callable from any thread (workers, timers, etc.). This is
    /// the landing pad for cross-thread native access (window mutations from
    /// worker Lua, dialogs, clipboard, …).
    void post(std::function<void()> fn);

    /// Nudge the main run loop to drain pending messages.
    /// Thread-safe: callable from any thread.
    void notify();

    /// Start main-runloop integration (drains on every iteration).
    /// Must be called from the main thread after the webview is created.
    void init();

    /// Tear down runloop integration. Drains remaining messages first.
    void shutdown();
  };

  // ── Free helpers ─────────────────────────────────────────────────────
  // Module code (forwardToMain, store, window) marshals onto the main thread
  // without an App* in scope. These reach the live dispatcher via the global
  // app pointer set by setDispatchApp().
  void setDispatchApp(App* app);
  void dispatchPost(std::function<void()> fn);
  void dispatchNotify();

}  // namespace coconut::core
#endif  // CORE_DISPATCHER_H
