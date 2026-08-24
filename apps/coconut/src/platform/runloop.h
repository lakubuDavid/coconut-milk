#ifndef COCONUT_PLATFORM_RUNLOOP_H
#define COCONUT_PLATFORM_RUNLOOP_H

/// @file runloop.h
///
/// Portable main-run-loop integration.
///
/// The dispatch pump registers a callback that is invoked on the MAIN
/// thread whenever the platform run loop wakes up (macOS: CFRunLoopSource
/// in common modes; other platforms: polling/no-op fallback until a native
/// integration lands). Other threads request a wakeup via runloopNotify().

namespace coconut::platform {

  using RunloopCallback = void (*)();

  /// Register `callback` to fire on the main thread on run-loop wakeups.
  /// Call once from the main thread during startup.
  void runloopInit(RunloopCallback callback);

  /// Remove the callback / release platform resources.
  void runloopShutdown();

  /// Wake the main run loop so the registered callback fires soon.
  /// Thread-safe — callable from any thread.
  void runloopNotify();

}  // namespace coconut::platform

#endif  // COCONUT_PLATFORM_RUNLOOP_H
