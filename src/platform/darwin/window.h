#ifndef COCONUT_PLATFORM_DARWIN_WINDOW_H
#define COCONUT_PLATFORM_DARWIN_WINDOW_H

#include "webview/api.h" // webview_t

namespace coconut {
  struct Config;

  namespace window {
    /// Apply macOS-native window style (frameless, transparent, etc.).
    /// Forward declaration — implementation lives in window_style.mm
    /// for correct ObjC++ calling conventions.
    void platformApplyWindowStyle(webview_t wv, Config* cfg);

    /// Install WKNavigationDelegate for external URL interception.
    /// Implementation in window_style.mm.
    void platformInstallNavDelegate(webview_t wv);

    /// Set window background color (0-1 range).
    void platformSetWindowBackgroundColor(webview_t wv, float r, float g, float b, float a);
  }
}

#endif // COCONUT_PLATFORM_DARWIN_WINDOW_H