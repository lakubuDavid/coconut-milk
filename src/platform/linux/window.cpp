/// Linux (GTK3) window style implementations.
///
/// Operates on the GtkWindow retrieved from the webview handle.

#include "window.h"
#include "../../config.h"
#include "../../debug.h"

#include <webview/webview.h>

#include <gtk/gtk.h>
#include <webkit2/webkit2.h>

#include <format>
#include <string>

namespace coconut::window {

/// Get the GtkWindow from webview, with error checking.
static GtkWindow* getWindow(webview_t wv) {
  if (!wv) {
    debug::error("window: webview handle is null");
    return nullptr;
  }
  void* win = webview_get_window(wv);
  if (!win) {
    debug::error("window: no native GtkWindow from webview");
    return nullptr;
  }
  return GTK_WINDOW(win);
}

// ── Window style (frameless, transparent) ───────────────────────────────

void platformApplyWindowStyle(webview_t wv, Config* cfg) {
  GtkWindow* win = getWindow(wv);
  if (!win) return;

  if (!cfg) {
    debug::warn("window::platformApplyWindowStyle: null config");
    return;
  }

  // Frameless window
  if (cfg->frameless) {
    gtk_window_set_decorated(win, FALSE);
    debug::info("window: set frameless (undecorated)");
  }

  // Transparent background — requires the window to support transparency
  // via RGBA visual.
  if (cfg->transparent) {
    GdkScreen* screen = gtk_window_get_screen(win);
    GdkVisual* rgba = gdk_screen_get_rgba_visual(screen);
    if (rgba) {
      gtk_widget_set_visual(GTK_WIDGET(win), rgba);
      debug::info("window: set RGBA visual for transparency");
    } else {
      debug::warn("window: RGBA visual not available; transparency may not work");
    }
  }
}

// ── Navigation delegate (URL interception) ──────────────────────────────

extern "C" gboolean coconut_nav_policy_decision(
    WebKitWebView* web_view, WebKitPolicyDecision* decision,
    WebKitPolicyDecisionType type, gpointer user_data) {
  (void)web_view; (void)user_data;

  if (type != WEBKIT_POLICY_DECISION_TYPE_NAVIGATION_ACTION) {
    return FALSE; // let other decision types pass through
  }

  WebKitNavigationAction* action =
      webkit_navigation_policy_decision_get_navigation_action(
          WEBKIT_NAVIGATION_POLICY_DECISION(decision));
  if (!action) return FALSE;

  const gchar* uri = webkit_uri_request_get_uri(
      webkit_navigation_action_get_request(action));
  if (!uri) return FALSE;

  std::string url(uri);

  // Allow coconut:// scheme — it's our custom protocol handled by the scheme handler.
  if (url.rfind("coconut://", 0) == 0) {
    return FALSE; // allow
  }

  // Open external URLs in the system browser instead of navigating.
  if (url.rfind("http://", 0) == 0 || url.rfind("https://", 0) == 0) {
    webkit_policy_decision_ignore(decision);
    // Use xdg-open to open in system browser
    std::string cmd = "xdg-open '" + url + "' &";
    if (std::system(cmd.c_str()) != 0) {
      debug::warn(std::format("window: failed to open external URL: {}", url));
    }
    return TRUE; // handled
  }

  // For other schemes (ftp, mailto, etc.), ignore and let the system handle
  webkit_policy_decision_ignore(decision);
  return TRUE;
}

void platformInstallNavDelegate(webview_t wv) {
  if (!wv) return;

  // Get the WebKitWebView from the webview handle.
  void* browser = webview_get_native_handle(
      wv, WEBVIEW_NATIVE_HANDLE_KIND_BROWSER_CONTROLLER);
  if (!browser) {
    debug::error("window: cannot install nav delegate — no WebKitWebView");
    return;
  }

  WebKitWebView* webview = WEBKIT_WEB_VIEW(browser);

  g_signal_connect(webview, "decide-policy",
      G_CALLBACK(coconut_nav_policy_decision), nullptr);

  debug::info("window: installed navigation policy delegate");
}

// ── Window background color ─────────────────────────────────────────────

void platformSetWindowBackgroundColor(webview_t wv, float r, float g, float b, float a) {
  GtkWindow* win = getWindow(wv);
  if (!win) return;

  // GTK3: use CSS to set background color on the window's child area.
  // Note: full transparency (alpha < 1.0) requires RGBA visual (set in ApplyWindowStyle).
  GtkWidget* widget = gtk_bin_get_child(GTK_BIN(win));
  if (!widget) widget = GTK_WIDGET(win);

  // Convert 0-1 float to 0-255 integer
  int ri = static_cast<int>(r * 255);
  int gi = static_cast<int>(g * 255);
  int bi = static_cast<int>(b * 255);
  int ai = static_cast<int>(a * 255);

  std::string css = std::format(
      "window {{ background: rgba({},{},{},{}); }}",
      ri, gi, bi, ai);

  GtkCssProvider* provider = gtk_css_provider_new();
  gtk_css_provider_load_from_data(provider, css.c_str(), -1, nullptr);

  GdkScreen* screen = gtk_window_get_screen(win);
  gtk_style_context_add_provider_for_screen(
      screen,
      GTK_STYLE_PROVIDER(provider),
      GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

  g_object_unref(provider);
}

} // namespace coconut::window
