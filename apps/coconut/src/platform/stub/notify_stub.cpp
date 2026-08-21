/// Stub: notify platform function.

#ifdef NO_PLATFORM

#include "platform/darwin/notify.h"
#include <string>

namespace coconut::notify {

bool platformNotify(const std::string& title, const std::string& body) {
  (void)title; (void)body;
  return false;
}

} // namespace coconut::notify

#endif // NO_PLATFORM
