/// Linux lifecycle stubs — no window event hooks yet.

#include "lifecycle.h"
#include "../../debug.h"

#include <iostream>

namespace coconut::lifecycle {

void platformRegisterEvents(App*) {
  debug::warn("Linux lifecycle hooks not yet implemented");
}

void platformUnregisterEvents() {
  // no-op
}

} // namespace coconut::lifecycle
