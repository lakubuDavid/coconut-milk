/// Platform dispatcher — includes the right lifecycle implementation
/// at compile time based on OS macros.

#include "lifecycle.h"
#include "debug.h"

// Select platform implementation
#if defined(__APPLE__)
  #include "platform/darwin/lifecycle.h"
#elif defined(_WIN32)
  #include "platform/win/lifecycle.h"
#elif defined(__linux__)
  #include "platform/linux/lifecycle.h"
#else
  #error "Unsupported platform — no lifecycle implementation available"
#endif

#include <iostream>

namespace coconut::lifecycle {

void registerEvents(App* app) {
  debug::info("registering platform hooks...");
  platformRegisterEvents(app);
}

void unregisterEvents() {
  platformUnregisterEvents();
}

} // namespace coconut::lifecycle
