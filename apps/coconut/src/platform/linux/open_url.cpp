/// Linux URL opener.
///
/// Uses gtk_show_uri_on_window() when possible (GTK3+), or falls back
/// to forking xdg-open for older environments or headless operation.

#include "open_url.h"

#include <gtk/gtk.h>

#include <cstdlib>
#include <string>

namespace coconut::open_url {

bool platformOpenUrl(const std::string& url) {
  if (url.empty()) return false;

  // Try GTK native URI opener. Pass nullptr for GtkWindow since we may
  // not have one available (called before window creation).
  GError* error = nullptr;
  if (gtk_show_uri_on_window(nullptr, url.c_str(), GDK_CURRENT_TIME, &error)) {
    return true;
  }

  // GTK failed — log and fall back to xdg-open.
  if (error) {
    g_warning("gtk_show_uri_on_window failed: %s", error->message);
    g_error_free(error);
  }

  // Fallback: fork xdg-open. This is more reliable in headless/CI
  // environments where there's no display manager.
  std::string cmd = "xdg-open '" + url + "'";
  int ret = std::system(cmd.c_str());
  return (ret == 0);
}

} // namespace coconut::open_url
