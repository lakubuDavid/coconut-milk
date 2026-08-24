#ifndef COCONUT_MODULES_STORE_H
#define COCONUT_MODULES_STORE_H

#include <sol/sol.hpp>
#include "thread_kind.h"

namespace coconut {
  struct App;
}

namespace coconut::modules {

  /// Register coconut.store.set / .get / .has / .delete / .clear / .keys.
  /// On Main, operates against app->bridge_state->store (resolved via
  /// lua["coconut"]["_app"]). On Background, operations forward onto the
  /// main run loop via forwardToMain against the injected App* — full
  /// parity, including store:update JS emission.
  void init_store(sol::state& lua, ThreadKind kind);

  /// Inject the App* used by Background forwarding (worker store ops are
  /// marshalled onto the main run loop against this instance). Call once
  /// during startup before worker pools are built.
  void setStoreApp(App* app);

}  // namespace coconut::modules

#endif
