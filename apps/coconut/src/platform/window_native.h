#ifndef COCONUT_PLATFORM_WINDOW_NATIVE_H
#define COCONUT_PLATFORM_WINDOW_NATIVE_H

/// @file window_native.h
///
/// Portable declarations for platform window operations.
///
/// Each platform directory (darwin/, win/, linux/) provides implementations
/// of these functions; the build system selects exactly one implementation
/// set per target OS. Tests may link platform/stub instead.
///
/// ALL of these must be called on the MAIN thread (AppKit/GTK requirement).

#include "webview/api.h"  // webview_t

#include <string>

namespace coconut {
  namespace window {

    /// Move window by (dx, dy) in screen coordinates.
    void platformMoveWindow(webview_t wv, int dx, int dy);

    /// Set absolute window position.
    void platformSetWindowPosition(webview_t wv, int x, int y);

    /// Get current window position.
    void platformGetWindowPosition(webview_t wv, int& x, int& y);

    /// Minimize / maximize window.
    void platformMinimizeWindow(webview_t wv);
    void platformMaximizeWindow(webview_t wv);

    /// Fullscreen controls.
    void platformToggleFullscreen(webview_t wv);
    void platformSetFullscreen(webview_t wv, bool on);

    /// Set movable by background (window drag from any content area).
    void platformSetMovableByBackground(webview_t wv, bool on);

    /// Set window background color (0-1 range).
    void platformSetWindowBackgroundColor(webview_t wv, float r, float g, float b, float a);

    /// Live title mutation.
    void platformSetWindowTitle(webview_t wv, const std::string& title);

    /// Live min/max size constraints.
    void platformSetMinimumWindowSize(webview_t wv, int w, int h);
    void platformSetMaximumWindowSize(webview_t wv, int w, int h);

    /// Toggle live resizability.
    void platformSetResizable(webview_t wv, bool on);

  }  // namespace window
}  // namespace coconut

#endif  // COCONUT_PLATFORM_WINDOW_NATIVE_H
