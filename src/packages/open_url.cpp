#include "open_url.h"

#if defined(__APPLE__)
  #include "platform/darwin/open_url.h"
#elif defined(_WIN32)
  #include "platform/win/open_url.h"
#elif defined(__linux__)
  #include "platform/linux/open_url.h"
#endif

namespace coconut::open_url {

bool open(const std::string& url) {
  return platformOpenUrl(url);
}

} // namespace coconut::open_url