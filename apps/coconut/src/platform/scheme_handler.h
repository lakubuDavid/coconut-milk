#ifndef COCONUT_PLATFORM_SCHEME_HANDLER_H
#define COCONUT_PLATFORM_SCHEME_HANDLER_H

/// @file scheme_handler.h
///
/// Platform dispatch for registering a custom `coconut://` URL scheme.
///
/// Each platform backend handles this differently:
///   - macOS (WKWebView): WKURLSchemeHandler (requires pre-webview hook)
///   - Windows (WebView2):  CoreWebView2.WebResourceRequested event
///   - Linux  (WebKitGTK):  webkit_web_context_register_uri_scheme()
///
/// On macOS the scheme handler must be installed BEFORE the webview is
/// created, because WKURLSchemeHandler is registered on the
/// WKWebViewConfiguration at construction time. Call
/// `installSchemeHandlerHook()` once at startup, before `webview_create()`.
///
/// On Windows and Linux the handler can be registered after the webview
/// exists, so `finalizeSchemeHandler()` does the actual work there.

#include <string>

// Use webview_t from the webview library (declared in webview/api.h as
// typedef struct webview* webview_t).
// We include the API header so callers (main.cpp) don't need to.
#include <webview/api.h>

namespace coconut::platform {

/// Install the pre-webview configuration hook.
/// On macOS this sets the cocoa_wkwebview_engine::on_configure_config
/// callback so WKURLSchemeHandler is registered at webview creation time.
/// On other platforms this is a no-op.
/// @param root_dir  The filesystem root to resolve coconut:// paths against.
void installSchemeHandlerHook(const std::string& root_dir);

/// Finalize scheme handler registration after the webview exists.
/// On macOS this is a no-op (already done via the hook).
/// On Windows/Linux this registers the handler with the native webview API.
/// @param wv  The webview instance.
/// @return true on success.
bool finalizeSchemeHandler(webview_t wv);

} // namespace coconut::platform

#endif // COCONUT_PLATFORM_SCHEME_HANDLER_H
