/// Stub: coconut:// scheme handler.
/// Real declarations come from platform/scheme_handler.h.

#ifdef NO_PLATFORM

#include "platform/scheme_handler.h"
#include <string>

namespace coconut::platform {

void installSchemeHandlerHook(const std::string& root_dir) {
  (void)root_dir;
}

bool finalizeSchemeHandler(webview_t wv) {
  (void)wv;
  return true;
}

} // namespace coconut::platform

#endif // NO_PLATFORM
