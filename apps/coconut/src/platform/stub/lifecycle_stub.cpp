/// Stub: lifecycle platform functions.

#ifdef NO_PLATFORM

#include "platform/darwin/lifecycle.h"

namespace coconut::lifecycle {

void platformRegisterEvents(App* app) {
  (void)app;
}

void platformUnregisterEvents() {
}

} // namespace coconut::lifecycle

#endif // NO_PLATFORM
