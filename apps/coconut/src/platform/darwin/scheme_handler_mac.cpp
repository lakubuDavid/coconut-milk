/// macOS-specific: sets the cocoa_wkwebview_engine::on_configure_config hook
/// so the coconut:// WKURLSchemeHandler is registered at webview creation time.
///
/// This file must be a .cpp (not .mm) because it includes the webview
/// library's template-heavy headers which don't compile cleanly in ObjC++ mode.
///
/// The actual WKURLSchemeHandler implementation lives in scheme_handler.mm.

#if defined(__APPLE__)

#include "platform/scheme_handler.h"

#include "debug.h"

#include <webview/webview.h>

#include <format>
#include <string>

namespace coconut::platform {

// Forward-declared in scheme_handler.mm
void registerWKURLSchemeHandler(void* config);
void setSchemeHandlerRoot(const std::string& root_dir);

void installSchemeHandlerHook(const std::string& root_dir) {
  setSchemeHandlerRoot(root_dir);

  // Access the cocoa_wkwebview_engine static inline member via the
  // webview library's header chain (included above).
  auto& hook = webview::detail::cocoa_webkit::cocoa_wkwebview_engine::on_configure_config;

  hook = [](void* config) {
    registerWKURLSchemeHandler(config);
  };

  debug::info(std::format("scheme_handler: on_configure_config hook set (root={})",
              root_dir));
}

bool finalizeSchemeHandler(webview_t wv) {
  // On macOS the scheme handler is already registered via the pre-webview
  // hook during webview_create(). This is a no-op.
  (void)wv;
  debug::info("scheme_handler: macOS finalize (no-op, hook did the work)");
  return true;
}

} // namespace coconut::platform

#endif // __APPLE__
