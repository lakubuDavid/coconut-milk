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

    // Thin pump (Phase 2): the core Dispatcher owns all message routing;
    // the CFRunLoopSource just wakes the main loop to flush it.
    if (app->dispatcher != nullptr) {
      app->dispatcher->flush();
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
