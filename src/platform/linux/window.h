#ifndef COCONUT_PLATFORM_LINUX_WINDOW_H
#define COCONUT_PLATFORM_LINUX_WINDOW_H

#include "webview/api.h" // webview_t

namespace coconut {
  struct Config;

  namespace window {
    /// Apply Linux-native window style (frameless, etc.) — stub.
    inline void platformApplyWindowStyle(webview_t /*wv*/, Config* /*cfg*/) {}

    /// Set window background color (0-1 range) — stub.
    inline void platformSetWindowBackgroundColor(webview_t /*wv*/, float, float, float, float) {}
  }
}

#endif // COCONUT_PLATFORM_LINUX_WINDOW_H
