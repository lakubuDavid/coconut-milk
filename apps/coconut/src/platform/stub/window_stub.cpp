/// Stub: window platform functions.
/// Real declarations come from the portable platform/window_native.h
/// (platform/darwin/window*.h remain as back-compat shims).

#ifdef NO_PLATFORM

#include <string>
#include "platform/darwin/window.h"
#include "platform/darwin/window_handle.h"

namespace coconut::window {

  // From platform/darwin/window.h
  void platformApplyWindowStyle(webview_t wv, Config* cfg) {
    (void)wv;
    (void)cfg;
  }

  void platformInstallNavDelegate(webview_t wv) {
    (void)wv;
  }

  void platformOpenDevTools(webview_t wv) {
    (void)wv;
  }

  void platformSetWindowBackgroundColor(webview_t wv, float r, float g, float b, float a) {
    (void)wv;
    (void)r;
    (void)g;
    (void)b;
    (void)a;
  }

  // From platform/darwin/window_handle.h
  void platformMoveWindow(webview_t wv, int dx, int dy) {
    (void)wv;
    (void)dx;
    (void)dy;
  }

  void platformSetWindowPosition(webview_t wv, int x, int y) {
    (void)wv;
    (void)x;
    (void)y;
  }

  void platformGetWindowPosition(webview_t wv, int& x, int& y) {
    (void)wv;
    x = 0;
    y = 0;
  }

  void platformMinimizeWindow(webview_t wv) {
    (void)wv;
  }

  void platformMaximizeWindow(webview_t wv) {
    (void)wv;
  }

  void platformToggleFullscreen(webview_t wv) {
    (void)wv;
  }

  void platformSetFullscreen(webview_t wv, bool on) {
    (void)wv;
    (void)on;
  }

  void platformSetMovableByBackground(webview_t wv, bool on) {
    (void)wv;
    (void)on;
  }

  // ── Live window mutations (window-module additions) ──────────────

  void platformSetWindowTitle(webview_t wv, const std::string& title) {
    (void)wv;
    (void)title;
  }

  void platformSetMinimumWindowSize(webview_t wv, int w, int h) {
    (void)wv;
    (void)w;
    (void)h;
  }

  void platformSetMaximumWindowSize(webview_t wv, int w, int h) {
    (void)wv;
    (void)w;
    (void)h;
  }

  void platformSetResizable(webview_t wv, bool on) {
    (void)wv;
    (void)on;
  }

}  // namespace coconut::window

#endif  // NO_PLATFORM
