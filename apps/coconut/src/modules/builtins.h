#ifndef COCONUT_MODULES_BUILTINS_H
#define COCONUT_MODULES_BUILTINS_H

#include <sol/sol.hpp>
#include "thread_kind.h"

namespace coconut::modules {

  /// Register the framework-level bind_mt commands (clipboard_*, dialog_*,
  /// fs_*, store_*, ping, getViews, __coconutWindowCtl, set_window_* …).
  ///
  /// These wrap the C++ functions bound on the coconut table and run on the
  /// MAIN thread only — Background is a no-op (workers call coconut.*
  /// directly; mt-only modules forward internally).
  void init_builtins(sol::state& lua, ThreadKind kind);

}  // namespace coconut::modules

#endif  // COCONUT_MODULES_BUILTINS_H
