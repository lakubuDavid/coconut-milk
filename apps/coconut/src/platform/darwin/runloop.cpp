/// macOS main-run-loop integration (CFRunLoopSource, common modes).
///
/// The source fires at "common modes" priority so it also runs during
/// modal tracking (menus, scroll views) — important for timely dispatch
/// of command results and view lifecycle events.

#include "../runloop.h"

#include <CoreFoundation/CFRunLoop.h>

namespace coconut::platform {

  namespace {
    RunloopCallback    s_callback       = nullptr;
    CFRunLoopSourceRef g_runloop_source = nullptr;
  }  // namespace

  void runloopInit(RunloopCallback callback) {
    s_callback = callback;

    CFRunLoopSourceContext ctx{};
    ctx.info    = nullptr;
    ctx.perform = [](void*) {
      if (s_callback)
        s_callback();
    };

    g_runloop_source = CFRunLoopSourceCreate(kCFAllocatorDefault, 0, &ctx);
    if (g_runloop_source != nullptr) {
      CFRunLoopAddSource(CFRunLoopGetMain(), g_runloop_source, kCFRunLoopCommonModes);
    }
  }

  void runloopShutdown() {
    if (g_runloop_source != nullptr) {
      CFRunLoopRemoveSource(CFRunLoopGetMain(), g_runloop_source, kCFRunLoopCommonModes);
      CFRelease(g_runloop_source);
      g_runloop_source = nullptr;
    }
    s_callback = nullptr;
  }

  void runloopNotify() {
    if (g_runloop_source != nullptr) {
      CFRunLoopSourceSignal(g_runloop_source);
      CFRunLoopWakeUp(CFRunLoopGetMain());
    }
  }

}  // namespace coconut::platform
