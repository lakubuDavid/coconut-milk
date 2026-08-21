/// Stub: open_url platform function.

#ifdef NO_PLATFORM

#include "platform/darwin/open_url.h"
#include <string>

namespace coconut::open_url {

bool platformOpenUrl(const std::string& url) {
  (void)url;
  return false;
}

} // namespace coconut::open_url

#endif // NO_PLATFORM
