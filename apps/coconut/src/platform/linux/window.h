#ifndef COCONUT_PLATFORM_LINUX_WINDOW_H
#define COCONUT_PLATFORM_LINUX_WINDOW_H

/// Linux (GTK3) window style functions.
///
/// Implementation lives in window.cpp to avoid pulling in GTK headers
/// into every translation unit that includes this header.

#include <webview/api.h> // webview_t

namespace coconut {
  struct Config;

  namespace window {
    /// Apply Linux-native window style (frameless, transparent, etc.).
    /// Uses gtk_window_set_decorated() for frameless mode.
    void platformApplyWindowStyle(webview_t wv, Config* cfg);

    /// Install WebKitGTK navigation policy for external URL interception.
    /// Connects to decide-policy-for-navigation-action signal.
    void platformInstallNavDelegate(webview_t wv);

    /// Open WebKitGTK inspector.
    void platformOpenDevTools(webview_t wv);

    /// Set window background color (0-1 range) using CSS/GTK background.
    void platformSetWindowBackgroundColor(webview_t wv, float r, float g, float b, float a);
  }
}

#endif // COCONUT_PLATFORM_LINUX_WINDOW_H
