#include "clipboard.h"

#if defined(__APPLE__)
  #include "platform/darwin/clipboard.h"
#elif defined(_WIN32)
  #include "platform/win/clipboard.h"
#elif defined(__linux__)
  #include "platform/linux/clipboard.h"
#endif

namespace coconut::clipboard {

std::string readText() {
  return platformReadText();
}

bool writeText(const std::string& text) {
  return platformWriteText(text);
}

} // namespace coconut::clipboard