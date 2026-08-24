#include "dispatch.h"
#include "app.h"
#include "bridge.h"
#include "debug.h"
#include "main_runtime.h"

#include <nlohmann/json.hpp>
#include "common.h"
#include "core/messages.h"  // JsRPCMessage

#include <deque>
#include <exception>
#include <format>
#include <mutex>

// Main-thread task queue backing dispatch::post(). Mutex + deque is plenty
// at this scale; tasks are drained wholesale on the main thread.
static std::mutex                        g_tasks_mutex;
static std::deque<std::function<void()>> g_main_tasks;

// ── Platform-specific run-loop integration ─────────────────────────────
// Runloop wakeup mechanics live behind the platform port (runloop.h):
//   darwin/runloop.cpp — CFRunLoopSource in common modes
//   stub/runloop.cpp   — polling fallback for other platforms
// dispatch.cpp only decides WHAT runs on the main thread; the port owns
// HOW the main loop gets woken.

#include "../platform/runloop.h"

namespace {

  /// The App pointer used by the platform run-loop callback.
  /// Set by coconut::dispatch::init(), cleared by shutdown().
  /// Only one App at a time per process.
  coconut::App* g_dispatch_app = nullptr;

}  // anonymous namespace

namespace coconut::dispatch {

  // ── Lifecycle ─────────────────────────────────────────────────────────

  void init(App* app) {
    if (app == nullptr) {
      return;
    }
    g_dispatch_app = app;

    platform::runloopInit([] { drain(g_dispatch_app); });
  }

  void shutdown(App* app) {
    // Drain any remaining messages before tearing down.
    drain(app);

    platform::runloopShutdown();

    g_dispatch_app = nullptr;
    (void)app;
  }

  void drain(App* app) {
    if (app == nullptr) {
      return;
    }

    // Thin pump (Phase 2): the core Dispatcher owns all message routing;
    // the CFRunLoopSource just wakes the main loop to flush it.
    if (app->dispatcher != nullptr) {
      app->dispatcher->flush();
    }

    // Run posted main-thread closures (worker→native marshalling, etc.).
    std::deque<std::function<void()>> local_tasks;
    {
      std::lock_guard<std::mutex> lock(g_tasks_mutex);
      local_tasks.swap(g_main_tasks);
    }
    while (!local_tasks.empty()) {
      auto fn = std::move(local_tasks.front());
      local_tasks.pop_front();
      try {
        if (fn) {
          fn();
        }
      } catch (const std::exception& e) {
        debug::error(std::format("dispatch: posted task threw: {}", e.what()));
      } catch (...) {
        debug::error("dispatch: posted task threw (unknown)");
      }
    }
  }

  void notify(App* app) {
    if (app == nullptr)
      return;
    platform::runloopNotify();
  }

  // ── Enqueue helpers ───────────────────────────────────────────────────

  void post(std::function<void()> fn) {
    if (!fn) {
      return;
    }
    {
      std::lock_guard<std::mutex> lock(g_tasks_mutex);
      g_main_tasks.push_back(std::move(fn));
    }
    notify(g_dispatch_app);  // wake the main run loop (no-op pre-init)
  }

  void evalJS(App* app, std::string_view js) {
    if (app == nullptr || app->bridge_state == nullptr || app->bridge_state->transport == nullptr) {
      return;
    }
    // Routed through the transport's eval() — no raw webview_eval here.
    app->bridge_state->transport->eval(std::string(js));
  }

  void lifecycleEvent(App* app, std::string_view view_name, std::string_view event_name) {
    if (app == nullptr || app->dispatcher == nullptr) {
      return;
    }
    // Routed through the core Dispatcher (main-loop flush calls it).
    app->dispatcher->queue(core::LifecycleMessage{
        .ViewName  = std::string(view_name),
        .EventName = std::string(event_name),
    });
  }

}  // namespace coconut::dispatch
