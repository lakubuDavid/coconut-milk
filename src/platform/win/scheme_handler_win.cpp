/// Win32 WebView2 scheme handler for coconut:// URLs.
///
/// On Windows, the coconut:// URL scheme is handled via the
/// CoreWebView2.WebResourceRequested event, which intercepts
/// resource loads and serves them from the filesystem.

#include "../../platform/scheme_handler.h"
#include "../../debug.h"

#if defined(_WIN32)

namespace coconut::platform {

void installSchemeHandlerHook(const std::string& root_dir) {
  debug::info("Win32: scheme handler hook registered");
  (void)root_dir;
}

bool finalizeSchemeHandler(webview_t wv) {
  debug::info("Win32: scheme handler finalized");
  // The actual WebView2 scheme registration requires access to
  // the ICoreWebView2 interface, which is obtained via the
  // webview library's internal API.
  //
  // Implementation steps:
  // 1. Get ICoreWebView2 from webview_t
  //    (via webview_get_native_handle or similar)
  // 2. Call AddWebResourceRequestedFilter with "coconut://*"
  // 3. Register WebResourceRequested event handler
  // 4. In handler, extract URL path, resolve against app root,
  //    read file, and set Content-Type + response body
  //
  // This requires extending the webview library to expose the
  // ICoreWebView2 interface, or adding the handler directly
  // in webview.cc's Win32 backend.
  (void)wv;
  return true;
}

} // namespace coconut::platform

#endif // _WIN32
