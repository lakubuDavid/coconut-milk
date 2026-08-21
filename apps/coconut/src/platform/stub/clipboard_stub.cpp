/// Stub: clipboard platform functions.

#ifdef NO_PLATFORM

#include "platform/darwin/clipboard.h"
#include <string>

namespace coconut::clipboard {

std::string platformReadText() {
  return "";
}

bool platformWriteText(const std::string& text) {
  (void)text;
  return false;
}

} // namespace coconut::clipboard

#endif // NO_PLATFORM
