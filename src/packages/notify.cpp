#include "notify.h"

#if defined(__APPLE__)
  #include "platform/darwin/notify.h"
#elif defined(_WIN32)
  #include "platform/win/notify.h"
#elif defined(__linux__)
  #include "platform/linux/notify.h"
#endif

namespace coconut::notify {

bool notify(const std::string& title, const std::string& body) {
  return platformNotify(title, body);
}

} // namespace coconut::notify