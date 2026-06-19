/// Linux (GTK3) window handle operations.
///
/// Implements window manipulation via GTK3 APIs:
///   - gtk_window_move / gtk_window_get_position for positioning
///   - gtk_window_iconify / gtk_window_maximize for state
///   - gtk_window_fullscreen / gtk_window_unfullscreen for fullscreen
///
/// The GtkWindow pointer is retrieved from the webview handle.

#include "window_handle.h"
#include "../../debug.h"

#include <webview/webview.h>

#include <gtk/gtk.h>

#include <format>

namespace coconut::window {

/// Get the GtkWindow from webview.
static GtkWindow* getWindow(webview_t wv) {
  if (!wv) {
    debug::error("window_handle: webview handle is null");
    return nullptr;
  }
  void* win = webview_get_window(wv);
  if (!win) {
    debug::error("window_handle: no native GtkWindow");
    return nullptr;
  }
  return GTK_WINDOW(win);
}

// ── Position ────────────────────────────────────────────────────────────

void platformMoveWindow(webview_t wv, int dx, int dy) {
  GtkWindow* win = getWindow(wv);
  if (!win) return;

  gint x = 0, y = 0;
  gtk_window_get_position(win, &x, &y);
  gtk_window_move(win, x + dx, y + dy);

  debug::log(std::format("window_handle: moveBy({}, {})", dx, dy));
}

void platformSetWindowPosition(webview_t wv, int x, int y) {
  GtkWindow* win = getWindow(wv);
  if (!win) return;

  gtk_window_move(win, x, y);
  debug::log(std::format("window_handle: setPosition({}, {})", x, y));
}

void platformGetWindowPosition(webview_t wv, int& x, int& y) {
  x = 0;
  y = 0;
  GtkWindow* win = getWindow(wv);
  if (!win) return;

  gint gx = 0, gy = 0;
  gtk_window_get_position(win, &gx, &gy);
  x = static_cast<int>(gx);
  y = static_cast<int>(gy);
}

// ── Window state ────────────────────────────────────────────────────────

void platformMinimizeWindow(webview_t wv) {
  GtkWindow* win = getWindow(wv);
  if (!win) return;
  gtk_window_iconify(win);
  debug::log("window_handle: minimized");
}

void platformMaximizeWindow(webview_t wv) {
  GtkWindow* win = getWindow(wv);
  if (!win) return;

  if (gtk_window_is_maximized(win)) {
    gtk_window_unmaximize(win);
  } else {
    gtk_window_maximize(win);
  }
  debug::log("window_handle: toggled maximize");
}

// ── Fullscreen ──────────────────────────────────────────────────────────

void platformToggleFullscreen(webview_t wv) {
  GtkWindow* win = getWindow(wv);
  if (!win) return;

  if (gtk_window_is_fullscreen(win)) {
    gtk_window_unfullscreen(win);
  } else {
    gtk_window_fullscreen(win);
  }
  debug::log("window_handle: toggled fullscreen");
}

void platformSetFullscreen(webview_t wv, bool on) {
  GtkWindow* win = getWindow(wv);
  if (!win) return;

  if (on) {
    gtk_window_fullscreen(win);
  } else {
    gtk_window_unfullscreen(win);
  }
  debug::log(std::format("window_handle: setFullscreen({})", on));
}

// ── Movable by background ───────────────────────────────────────────────

void platformSetMovableByBackground(webview_t wv, bool on) {
  GtkWindow* win = getWindow(wv);
  if (!win) return;

  // GTK3: GtkWindow doesn't have a direct "movable by background" property
  // like macOS NSWindow.setMovableByBackground. On Linux, this is typically
  // achieved by handling mouse events on the window background and calling
  // gtk_window_begin_move_drag(). This is a stub that logs the intent.
  //
  // A complete implementation would connect to button-press-event and
  // motion-notify-event to drag the window from content areas.
  debug::log(std::format("window_handle: setMovableByBackground({}) — "
                          "partial (GTK3 has no native equivalent)", on));
  (void)on;
}

// ── Background color ────────────────────────────────────────────────────

void platformSetWindowBackgroundColor(webview_t wv, float r, float g, float b, float a) {
  // Delegates to the window.cpp implementation for CSS-based background.
  // We include the same logic here to avoid cross-file dependency issues.
  GtkWindow* win = getWindow(wv);
  if (!win) return;

  GtkWidget* widget = gtk_bin_get_child(GTK_BIN(win));
  if (!widget) widget = GTK_WIDGET(win);

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
