/// Win32 lifecycle stubs — no window event hooks yet.

#include "lifecycle.h"

#include <iostream>

namespace coconut::lifecycle {

void platformRegisterEvents(App*) {
  std::cerr << "[lifecycle] Win32 lifecycle hooks not yet implemented\n";
}

void platformUnregisterEvents() {
  // no-op
}

} // namespace coconut::lifecycle
