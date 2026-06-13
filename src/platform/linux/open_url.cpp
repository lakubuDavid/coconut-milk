#include "open_url.h"

namespace coconut::open_url {

bool platformOpenUrl(const std::string& url) {
  // TODO: implement with xdg-open
  (void)url;
  return false;
}

} // namespace coconut::open_url