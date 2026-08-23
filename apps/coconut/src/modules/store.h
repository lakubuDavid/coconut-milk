#ifndef COCONUT_MODULES_STORE_H
#define COCONUT_MODULES_STORE_H

#include <sol/sol.hpp>
#include "thread_kind.h"

namespace coconut {
  struct App;
}

namespace coconut::modules {

  /// Register coconut.store.set / .get / .has / .delete / .clear / .keys.
  /// On Main, needs access to app->bridge_state->store. The caller should
  /// set lua["coconut"]["_app"] = (App*) before calling this on the main thread.
  /// On Background, registers stubs.
  void init_store(sol::state& lua, ThreadKind kind);

  /// Inject the App* used by Background forwarding (worker store ops are
  /// marshalled onto the main run loop against this instance). Call once
  /// during startup before worker pools are built.
  void setStoreApp(App* app);

}  // namespace coconut::modules

#endif
