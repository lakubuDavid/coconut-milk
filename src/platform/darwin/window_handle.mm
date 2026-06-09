#include "platform/darwin/window_handle.h"

#include "debug.h"

#import <Cocoa/Cocoa.h>

namespace coconut::window {

static NSWindow* getNSWindow(webview_t wv) {
  if (!wv) return nil;
  return (__bridge NSWindow*)webview_get_window(wv);
}

void platformMoveWindow(webview_t wv, int dx, int dy) {
  NSWindow* win = getNSWindow(wv);
  if (!win) {
    debug::warn("platformMoveWindow: no native window");
    return;
  }
  NSRect frame = [win frame];
  frame.origin.x += dx;
  frame.origin.y += dy;
  [win setFrameOrigin:frame.origin];
  debug::info(std::string("platformMoveWindow: dx=") + std::to_string(dx)
              + " dy=" + std::to_string(dy)
              + " new=(" + std::to_string((int)frame.origin.x)
              + "," + std::to_string((int)frame.origin.y) + ")");
}

void platformSetWindowPosition(webview_t wv, int x, int y) {
  NSWindow* win = getNSWindow(wv);
  if (!win) {
    debug::warn("platformSetWindowPosition: no native window");
    return;
  }
  // setFrameTopLeftPoint uses top-left screen coordinates (y=0 = top of screen).
  NSPoint pt = NSMakePoint(x, y);
  [win setFrameTopLeftPoint:pt];
  debug::info(std::string("platformSetWindowPosition: x=") + std::to_string(x)
              + " y=" + std::to_string(y));
}

void platformGetWindowPosition(webview_t wv, int& x, int& y) {
  x = y = 0;
  NSWindow* win = getNSWindow(wv);
  if (!win) {
    debug::warn("platformGetWindowPosition: no native window");
    return;
  }
  NSRect frame = [win frame];
  x = (int)frame.origin.x;
  y = (int)frame.origin.y;
}

void platformMinimizeWindow(webview_t wv) {
  NSWindow* win = getNSWindow(wv);
  if (!win) return;
  [win miniaturize:nil];
}

void platformMaximizeWindow(webview_t wv) {
  NSWindow* win = getNSWindow(wv);
  if (!win) return;
  [win performZoom:nil];
}

void platformToggleFullscreen(webview_t wv) {
  NSWindow* win = getNSWindow(wv);
  if (!win) return;
  [win toggleFullScreen:nil];
}

void platformSetFullscreen(webview_t wv, bool on) {
  NSWindow* win = getNSWindow(wv);
  if (!win) return;
  bool isFull = ([win styleMask] & NSWindowStyleMaskFullScreen) != 0;
  if (isFull != on) {
    [win toggleFullScreen:nil];
  }
}

void platformSetMovableByBackground(webview_t wv, bool on) {
  NSWindow* win = getNSWindow(wv);
  if (!win) return;
  win.movableByWindowBackground = on ? YES : NO;
}

} // namespace coconut::window
