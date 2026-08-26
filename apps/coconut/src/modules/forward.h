#ifndef COCONUT_MODULES_FORWARD_H
#define COCONUT_MODULES_FORWARD_H

/// @file forward.h
///
/// Main-thread forwarding helper for background-thread module impls.
///
/// Native APIs that require the main thread (NSWindow, NSPasteboard,
/// UNUserNotificationCenter, dialogs, …) are callable from worker Lua
/// by wrapping the native operation in forwardToMain(): the closure is
/// posted onto the main run loop (core::dispatchPost) and the worker blocks
/// until it completes. Safe because the main loop never blocks on
/// workers.

#include "../core/dispatcher.h"

#include <functional>
#include <future>
#include <type_traits>
#include <utility>

namespace coconut::modules {

  template <typename F>
  auto forwardToMain(F&& op) {
    using R = std::invoke_result_t<F>;
    std::packaged_task<R()> task(std::forward<F>(op));
    auto                    fut = task.get_future();
    coconut::core::dispatchPost([&task] { task(); });
    fut.wait();
    if constexpr (!std::is_void_v<R>) {
      return fut.get();
    }
  }

}  // namespace coconut::modules

#endif  // COCONUT_MODULES_FORWARD_H
