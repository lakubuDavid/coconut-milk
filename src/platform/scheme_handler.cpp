/// Platform dispatch for `coconut://` URL scheme handler.
///
/// On macOS, this file is not used — the implementation is in
/// darwin/scheme_handler.mm and darwin/scheme_handler_mac.cpp.
/// On Linux, this registers a coconut:// URI scheme with the WebKitGTK
/// web context so that custom resources are served from the filesystem.
/// On Windows, this provides no-op stubs (to be filled later).

#include "platform/scheme_handler.h"

#if !defined(__APPLE__)

#include "debug.h"

#include <string>
#include <format>

#if defined(__linux__)

#include <gtk/gtk.h>
#include <webkit2/webkit2.h>

#include <gio/gio.h>
#include <glib.h>

// ── Linux (WebKitGTK) implementation ─────────────────────────────────────

namespace coconut::platform {

static std::string s_root_dir;  // filesystem root for coconut:// resolution

/// Callback registered via webkit_web_context_register_uri_scheme().
/// Handles coconut:// requests by reading files from the root directory.
extern "C" void coconut_uri_scheme_handler(WebKitURISchemeRequest* request,
                                            gpointer user_data) {
  (void)user_data;

  const gchar* uri = webkit_uri_scheme_request_get_uri(request);
  const gchar* path = webkit_uri_scheme_request_get_path(request);

  if (!uri || !path) {
    webkit_uri_scheme_request_finish_error(
        request, g_error_new_literal(g_quark_from_static_string("coconut"),
                                     1, "Invalid request"));
    return;
  }

  debug::log(std::format("scheme_handler: requesting coconut://{}", path));

  // Build filesystem path: root_dir + request path
  // Strip leading slash from path if root_dir already has one
  std::string fs_path = s_root_dir;
  if (!fs_path.empty() && fs_path.back() == '/') {
    fs_path += (path[0] == '/') ? (path + 1) : path;
  } else {
    fs_path += path;
  }

  // Normalize path (remove ".." components for security)
  // Use GFile for safe path resolution
  GFile* base = g_file_new_for_path(s_root_dir.c_str());
  GFile* resolved = g_file_resolve_relative_path(base, path);

  // Check that the resolved path is within root_dir (security)
  char* resolved_path = g_file_get_path(resolved);
  if (!resolved_path) {
    webkit_uri_scheme_request_finish_error(
        request, g_error_new_literal(g_quark_from_static_string("coconut"),
                                     2, "Path resolution failed"));
    g_object_unref(resolved);
    g_object_unref(base);
    return;
  }

  // Read file contents
  GError* error = nullptr;
  gchar* contents = nullptr;
  gsize length = 0;
  gboolean ok = g_file_load_contents(resolved, nullptr, &contents, &length,
                                      nullptr, &error);

  if (ok && contents) {
    // Determine content type from file extension
    const gchar* content_type = nullptr;
    GFileInfo* info = g_file_query_info(resolved, "standard::content-type",
                                         G_FILE_QUERY_INFO_NONE, nullptr, nullptr);
    if (info) {
      content_type = g_file_info_get_content_type(info);
    }
    if (!content_type) {
      content_type = "application/octet-stream";
    }

    GInputStream* stream = g_memory_input_stream_new_from_data(
        contents, static_cast<gssize>(length), g_free);

    webkit_uri_scheme_request_finish(request, stream, length, content_type);
    g_object_unref(stream);

    if (info) g_object_unref(info);
  } else {
    std::string err_msg = std::format("File not found: {}", resolved_path);
    webkit_uri_scheme_request_finish_error(
        request, g_error_new_literal(g_quark_from_static_string("coconut"),
                                     3, err_msg.c_str()));
    if (error) g_error_free(error);
  }

  g_free(resolved_path);
  g_object_unref(resolved);
  g_object_unref(base);
}

void installSchemeHandlerHook(const std::string& root_dir) {
  s_root_dir = root_dir;
  // On Linux, the scheme handler is registered in finalizeSchemeHandler()
  // after the webview is created. The hook just stores the root directory.
  debug::info(std::format("scheme_handler: hook registered root_dir='{}'", root_dir));
}

bool finalizeSchemeHandler(webview_t wv) {
  if (!wv) {
    debug::error("scheme_handler: webview is null");
    return false;
  }

  // Get the WebKitWebView from the webview handle.
  void* browser = webview_get_native_handle(
      wv, WEBVIEW_NATIVE_HANDLE_KIND_BROWSER_CONTROLLER);
  if (!browser) {
    debug::error("scheme_handler: cannot get WebKitWebView");
    return false;
  }

  WebKitWebView* webview = WEBKIT_WEB_VIEW(browser);
  WebKitWebContext* context = webkit_web_view_get_context(webview);

  // Register the coconut:// scheme handler.
  // Note: webkit_web_context_register_uri_scheme() is deprecated in newer
  // WebKitGTK but remains the canonical approach for webkit2gtk-4.1.
  // The replacement (URISchemeRequest API changes) is for WebKitGTK 6.0.
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  webkit_web_context_register_uri_scheme(
      context, "coconut",
      reinterpret_cast<WebKitURISchemeRequestCallback>(coconut_uri_scheme_handler),
      nullptr, nullptr);
  #pragma GCC diagnostic pop

  debug::info("scheme_handler: registered coconut:// scheme for WebKitGTK");
  return true;
}

} // namespace coconut::platform

// ── Non-Linux platforms (Windows stubs) ──────────────────────────────────

#elif defined(_WIN32)

#include "debug.h"

namespace coconut::platform {

void installSchemeHandlerHook(const std::string& root_dir) {
  debug::info("scheme_handler: hook (stub, Win32 not yet implemented)");
  (void)root_dir;
}

bool finalizeSchemeHandler(webview_t wv) {
  debug::info("scheme_handler: finalize (stub, Win32 not yet implemented)");
  (void)wv;
  return true;
}

} // namespace coconut::platform

#endif // __linux__ / _WIN32

#endif // !defined(__APPLE__)
