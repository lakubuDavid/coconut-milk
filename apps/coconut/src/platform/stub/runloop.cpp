/// Fallback main-run-loop integration (non-macOS).
///
/// Without a native integration, posted tasks and dispatcher messages are
/// only flushed when something else runs drain() on the main thread.
/// Implement against GMainLoop (Linux) / a hidden message window
/// (Windows) to get true wakeups.

#include "../runloop.h"

#include "../../debug.h"

#include <atomic>

namespace coconut::platform {

  namespace {
    std::atomic<bool> s_warned{false};
  }  // namespace

  void runloopInit(RunloopCallback callback) {
    (void)callback;
    if (!s_warned.exchange(true)) {
      debug::warn(
          "runloop: no native integration on this platform; "
          "main-thread wakeups are not automatic"
      );
    }
  }

  void runloopShutdown() {
  }

  void runloopNotify() {
    // Nothing to signal — callers relying on wakeups will see delayed
    // flushing until a native integration is implemented.
  }

}  // namespace coconut::platform
