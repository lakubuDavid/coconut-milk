#ifndef COCONUT_MODULES_THREAD_KIND_H
#define COCONUT_MODULES_THREAD_KIND_H

namespace coconut::modules {

enum class ThreadKind {
  Main,        ///< Main thread — all APIs available (dialog, notify, clipboard, etc.)
  Background,  ///< Background thread — only thread-safe APIs; main-only APIs become stubs
};

} // namespace coconut::modules

#endif
