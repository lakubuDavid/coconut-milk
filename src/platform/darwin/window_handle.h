#ifndef COCONUT_PLATFORM_DARWIN_WINDOW_HANDLE_H
#define COCONUT_PLATFORM_DARWIN_WINDOW_HANDLE_H

#include "webview/api.h" // webview_t

namespace coconut {
  namespace window {
    /// Move window by (dx, dy) in screen coordinates (bottom-left origin).
    /// Uses [NSWindow setFrameOrigin:] with proper ObjC calling convention.
    void platformMoveWindow(webview_t wv, int dx, int dy);

    /// Set absolute window position (top-left screen coordinates).
    void platformSetWindowPosition(webview_t wv, int x, int y);

    /// Get current window position {x, y} (bottom-left origin).
    void platformGetWindowPosition(webview_t wv, int& x, int& y);

    /// Minimize window.
    void platformMinimizeWindow(webview_t wv);

    /// Zoom / maximize window.
    void platformMaximizeWindow(webview_t wv);

    /// Toggle fullscreen.
    void platformToggleFullscreen(webview_t wv);

    /// Set fullscreen on/off.
    void platformSetFullscreen(webview_t wv, bool on);

    /// Set movable by background (window drag from any content area).
    void platformSetMovableByBackground(webview_t wv, bool on);

    /// Set window background color (0-1 range).
    void platformSetWindowBackgroundColor(webview_t wv, float r, float g, float b, float a);
  }
}

#endif // COCONUT_PLATFORM_DARWIN_WINDOW_HANDLE_H
