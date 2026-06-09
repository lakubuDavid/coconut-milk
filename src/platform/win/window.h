#ifndef COCONUT_PLATFORM_WIN_WINDOW_H
#define COCONUT_PLATFORM_WIN_WINDOW_H

#include "webview/api.h" // webview_t

namespace coconut {
  struct Config;

  namespace window {
    /// Apply Windows-native window style (frameless, etc.) — stub.
    inline void platformApplyWindowStyle(webview_t /*wv*/, Config* /*cfg*/) {}
  }
}

#endif // COCONUT_PLATFORM_WIN_WINDOW_H
