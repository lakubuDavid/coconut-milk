#ifndef COCONUT_PLATFORM_LINUX_WINDOW_HANDLE_H
#define COCONUT_PLATFORM_LINUX_WINDOW_HANDLE_H

/// Linux (GTK3) window handle manipulation.
///
/// Wraps gtk_window_move, gtk_window_resize, gtk_window_fullscreen, etc.
/// The GtkWindow pointer is retrieved from the webview instance.

#include <webview/api.h> // webview_t

namespace coconut {
  namespace window {
    /// Move window by (dx, dy) pixels relative to current position.
    void platformMoveWindow(webview_t wv, int dx, int dy);

    /// Set absolute window position (top-left of screen).
    void platformSetWindowPosition(webview_t wv, int x, int y);

    /// Get current window position {x, y}.
    void platformGetWindowPosition(webview_t wv, int& x, int& y);

    /// Minimize (iconify) window.
    void platformMinimizeWindow(webview_t wv);

    /// Maximize window.
    void platformMaximizeWindow(webview_t wv);

    /// Toggle fullscreen state.
    void platformToggleFullscreen(webview_t wv);

    /// Set fullscreen on/off.
    void platformSetFullscreen(webview_t wv, bool on);

    /// Set movable by background (window drag from any content area).
    /// Uses GTK's gtk_window_begin_move_drag or CSS -GtkWindow-content-area.
    void platformSetMovableByBackground(webview_t wv, bool on);

    /// Set window background color (0-1 range). Delegates to window.cpp.
    void platformSetWindowBackgroundColor(webview_t wv, float r, float g, float b, float a);
  }
}

#endif // COCONUT_PLATFORM_LINUX_WINDOW_HANDLE_H
