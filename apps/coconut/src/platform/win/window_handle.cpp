/// Windows (Win32) window handle operations — stub implementations.
///
/// Windows support is aspirational; these provide linker-satisfying
/// definitions that warn instead of mutating a window. Implement against
/// win32 APIs (SetWindowText, GetSystemMetrics/WM_GETMINMAXINFO,
/// AdjustWindowRectEx...) when the platform target comes online.

#include "window_handle.h"

#include "../../debug.h"

namespace coconut::window {

#define WIN_STUB(name)                                               \
  do {                                                               \
    debug::warn("platform" name ": not implemented on Windows yet"); \
  } while (0)

  void platformMoveWindow(webview_t wv, int dx, int dy) {
    (void)wv;
    (void)dx;
    (void)dy;
    WIN_STUB("MoveWindow");
  }

  void platformSetWindowPosition(webview_t wv, int x, int y) {
    (void)wv;
    (void)x;
    (void)y;
    WIN_STUB("SetWindowPosition");
  }

  void platformGetWindowPosition(webview_t wv, int& x, int& y) {
    (void)wv;
    x = 0;
    y = 0;
  }

  void platformMinimizeWindow(webview_t wv) {
    (void)wv;
    WIN_STUB("MinimizeWindow");
  }

  void platformMaximizeWindow(webview_t wv) {
    (void)wv;
    WIN_STUB("MaximizeWindow");
  }

  void platformToggleFullscreen(webview_t wv) {
    (void)wv;
    WIN_STUB("ToggleFullscreen");
  }

  void platformSetFullscreen(webview_t wv, bool on) {
    (void)wv;
    (void)on;
    WIN_STUB("SetFullscreen");
  }

  void platformSetMovableByBackground(webview_t wv, bool on) {
    (void)wv;
    (void)on;
    WIN_STUB("SetMovableByBackground");
  }

  void platformSetWindowBackgroundColor(webview_t wv, float r, float g, float b, float a) {
    (void)wv;
    (void)r;
    (void)g;
    (void)b;
    (void)a;
    WIN_STUB("SetWindowBackgroundColor");
  }

  void platformSetWindowTitle(webview_t wv, const std::string& title) {
    (void)wv;
    (void)title;
    WIN_STUB("SetWindowTitle");
  }

  void platformSetMinimumWindowSize(webview_t wv, int w, int h) {
    (void)wv;
    (void)w;
    (void)h;
    WIN_STUB("SetMinimumWindowSize");
  }

  void platformSetMaximumWindowSize(webview_t wv, int w, int h) {
    (void)wv;
    (void)w;
    (int)h;
    WIN_STUB("SetMaximumWindowSize");
  }

  void platformSetResizable(webview_t wv, bool on) {
    (void)wv;
    (void)on;
    WIN_STUB("SetResizable");
  }

}  // namespace coconut::window
