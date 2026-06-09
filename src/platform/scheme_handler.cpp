/// Platform dispatch for `coconut://` URL scheme handler.
///
/// On macOS, this file is not used — the implementation is in
/// darwin/scheme_handler.mm and darwin/scheme_handler_mac.cpp.
/// On other platforms, this provides no-op stubs.

#include "platform/scheme_handler.h"

#if !defined(__APPLE__)

#include "debug.h"

namespace coconut::platform {

void installSchemeHandlerHook(const std::string& root_dir) {
  debug::info("scheme_handler: hook (stub, not yet implemented on this platform)");
  (void)root_dir;
}

bool finalizeSchemeHandler(webview_t wv) {
  debug::info("scheme_handler: finalize (stub, not yet implemented on this platform)");
  (void)wv;
  return true;
}

} // namespace coconut::platform

#endif // !defined(__APPLE__)
