/// Stub: window platform functions.
/// Real declarations come from platform/darwin/window.h and
/// platform/darwin/window_handle.h.

#ifdef NO_PLATFORM

#include "platform/darwin/window.h"
#include "platform/darwin/window_handle.h"
#include <string>

namespace coconut::window {

// From platform/darwin/window.h
void platformApplyWindowStyle(webview_t wv, Config* cfg) {
  (void)wv; (void)cfg;
}

void platformInstallNavDelegate(webview_t wv) {
  (void)wv;
}

void platformOpenDevTools(webview_t wv) {
  (void)wv;
}

void platformSetWindowBackgroundColor(webview_t wv, float r, float g, float b, float a) {
  (void)wv; (void)r; (void)g; (void)b; (void)a;
}

// From platform/darwin/window_handle.h
void platformMoveWindow(webview_t wv, int dx, int dy) {
  (void)wv; (void)dx; (void)dy;
}

void platformSetWindowPosition(webview_t wv, int x, int y) {
  (void)wv; (void)x; (void)y;
}

void platformGetWindowPosition(webview_t wv, int& x, int& y) {
  (void)wv; x = 0; y = 0;
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
  (void)wv; (void)on;
}

void platformSetMovableByBackground(webview_t wv, bool on) {
  (void)wv; (void)on;
}

} // namespace coconut::window

#endif // NO_PLATFORM
