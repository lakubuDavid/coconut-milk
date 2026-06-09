#ifndef COCONUT_PLATFORM_DARWIN_WINDOW_H
#define COCONUT_PLATFORM_DARWIN_WINDOW_H

#include "webview/api.h" // webview_t

namespace coconut {
  struct Config;

  namespace window {
    /// Apply macOS-native window style (frameless, transparent, etc.)
    /// using the raw Objective-C runtime.
    void platformApplyWindowStyle(webview_t wv, Config* cfg);
  }
}

#endif // COCONUT_PLATFORM_DARWIN_WINDOW_H
