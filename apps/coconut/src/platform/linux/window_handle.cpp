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

// Track fullscreen state manually (gtk_window_is_fullscreen not available in older GTK3)
static bool s_is_fullscreen = false;

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
    if (!win)
      return;

    gint x = 0, y = 0;
    gtk_window_get_position(win, &x, &y);
    gtk_window_move(win, x + dx, y + dy);

    debug::log(std::format("window_handle: moveBy({}, {})", dx, dy));
  }

  void platformSetWindowPosition(webview_t wv, int x, int y) {
    GtkWindow* win = getWindow(wv);
    if (!win)
      return;

    gtk_window_move(win, x, y);
    debug::log(std::format("window_handle: setPosition({}, {})", x, y));
  }

  void platformGetWindowPosition(webview_t wv, int& x, int& y) {
    x              = 0;
    y              = 0;
    GtkWindow* win = getWindow(wv);
    if (!win)
      return;

    gint gx = 0, gy = 0;
    gtk_window_get_position(win, &gx, &gy);
    x = static_cast<int>(gx);
    y = static_cast<int>(gy);
  }

  // ── Window state ────────────────────────────────────────────────────────

  void platformMinimizeWindow(webview_t wv) {
    GtkWindow* win = getWindow(wv);
    if (!win)
      return;
    gtk_window_iconify(win);
    debug::log("window_handle: minimized");
  }

  void platformMaximizeWindow(webview_t wv) {
    GtkWindow* win = getWindow(wv);
    if (!win)
      return;

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
    if (!win)
      return;

    if (s_is_fullscreen) {
      gtk_window_unfullscreen(win);
      s_is_fullscreen = false;
    } else {
      gtk_window_fullscreen(win);
      s_is_fullscreen = true;
    }
    debug::log("window_handle: toggled fullscreen");
  }

  void platformSetFullscreen(webview_t wv, bool on) {
    GtkWindow* win = getWindow(wv);
    if (!win)
      return;

    if (on) {
      gtk_window_fullscreen(win);
    } else {
      gtk_window_unfullscreen(win);
    }
    s_is_fullscreen = on;
    debug::log(std::format("window_handle: setFullscreen({})", on));
  }

  // ── Movable by background ───────────────────────────────────────────────

  void platformSetMovableByBackground(webview_t wv, bool on) {
    GtkWindow* win = getWindow(wv);
    if (!win)
      return;

    // GTK3: GtkWindow doesn't have a direct "movable by background" property
    // like macOS NSWindow.setMovableByBackground. On Linux, this is typically
    // achieved by handling mouse events on the window background and calling
    // gtk_window_begin_move_drag(). This is a stub that logs the intent.
    //
    // A complete implementation would connect to button-press-event and
    // motion-notify-event to drag the window from content areas.
    debug::log(std::format(
        "window_handle: setMovableByBackground({}) — "
        "partial (GTK3 has no native equivalent)",
        on
    ));
    (void)on;
  }

  // ── Background color ────────────────────────────────────────────────────
  // Implemented in window.cpp to avoid duplicate symbol. This header declares
  // it for API consistency; the implementation lives alongside other window
  // style functions.

  void platformSetWindowTitle(webview_t wv, const std::string& title) {
    GtkWindow* win = getWindow(wv);
    if (!win)
      return;
    gtk_window_set_title(win, title.c_str());
  }

  void platformSetMinimumWindowSize(webview_t wv, int w, int h) {
    GtkWindow* win = getWindow(wv);
    if (!win)
      return;
    GdkGeometry geom{};
    geom.min_width  = w;
    geom.min_height = h;
    gtk_window_set_geometry_hints(
        win, nullptr, &geom, static_cast<GdkWindowHints>(GDK_HINT_MIN_SIZE)
    );
  }

  void platformSetMaximumWindowSize(webview_t wv, int w, int h) {
    GtkWindow* win = getWindow(wv);
    if (!win)
      return;
    GdkGeometry geom{};
    geom.max_width  = w;
    geom.max_height = h;
    gtk_window_set_geometry_hints(
        win, nullptr, &geom, static_cast<GdkWindowHints>(GDK_HINT_MAX_SIZE)
    );
  }

  void platformSetResizable(webview_t wv, bool on) {
    GtkWindow* win = getWindow(wv);
    if (!win)
      return;
    gtk_window_set_resizable(win, on ? TRUE : FALSE);
  }

}  // namespace coconut::window
