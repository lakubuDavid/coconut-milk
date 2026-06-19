#ifndef COCONUT_PLATFORM_WIN_WINDOW_HANDLE_H
#define COCONUT_PLATFORM_WIN_WINDOW_HANDLE_H

#include "webview/api.h" // webview_t

namespace coconut {
  namespace window {
    /// Move window by (dx, dy) in screen coordinates.
    void platformMoveWindow(webview_t wv, int dx, int dy);

    /// Set absolute window position.
    void platformSetWindowPosition(webview_t wv, int x, int y);

    /// Get current window position {x, y}.
    void platformGetWindowPosition(webview_t wv, int& x, int& y);

    /// Minimize window.
    void platformMinimizeWindow(webview_t wv);

    /// Maximize window.
    void platformMaximizeWindow(webview_t wv);

    /// Restore window from minimized/maximized.
    void platformRestoreWindow(webview_t wv);

    /// Toggle fullscreen.
    void platformToggleFullscreen(webview_t wv);

    /// Set fullscreen on/off.
    void platformSetFullscreen(webview_t wv, bool on);

    /// Set movable by background (not directly supported on Win32).
    void platformSetMovableByBackground(webview_t wv, bool on);

    /// Set window background color (0-1 range).
    void platformSetWindowBackgroundColor(webview_t wv, float r, float g, float b, float a);
  }
}

#endif // COCONUT_PLATFORM_WIN_WINDOW_HANDLE_H
