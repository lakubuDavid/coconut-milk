#ifndef COCONUT_MODULES_NOTIFY_H
#define COCONUT_MODULES_NOTIFY_H

#include <sol/sol.hpp>
#include "thread_kind.h"

#if defined(__APPLE__)
#include "platform/darwin/notify.h"
#elif defined(_WIN32)
#include "platform/win/notify.h"
#elif defined(__linux__)
#include "platform/linux/notify.h"
#else
#error "Unsupported platform — no notify.h" implementation available"
#endif

namespace coconut::modules {

  /// Register coconut.notify.
  /// On Background, registers a stub.
  void init_notify(sol::state& lua, ThreadKind kind);

}  // namespace coconut::modules

#endif
