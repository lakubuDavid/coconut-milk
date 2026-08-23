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
  debug::log(std::string("platformMoveWindow: dx=") + std::to_string(dx)
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
  debug::log(std::string("platformSetWindowPosition: x=") + std::to_string(x)
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

void platformSetWindowBackgroundColor(webview_t wv, float r, float g, float b, float a) {
  NSWindow* win = getNSWindow(wv);
  if (!win) return;
  win.backgroundColor = [NSColor colorWithRed:r green:g blue:b alpha:a];
}

void platformSetWindowTitle(webview_t wv, const std::string& title) {
  NSWindow* win = getNSWindow(wv);
  if (!win) {
    debug::warn("platformSetWindowTitle: no native window");
    return;
  }
  [win setTitle:[NSString stringWithUTF8String:title.c_str()]];
}

void platformSetMinimumWindowSize(webview_t wv, int w, int h) {
  NSWindow* win = getNSWindow(wv);
  if (!win) {
    debug::warn("platformSetMinimumWindowSize: no native window");
    return;
  }
  [win setMinSize:NSMakeSize(w, h)];
}

void platformSetMaximumWindowSize(webview_t wv, int w, int h) {
  NSWindow* win = getNSWindow(wv);
  if (!win) {
    debug::warn("platformSetMaximumWindowSize: no native window");
    return;
  }
  [win setMaxSize:NSMakeSize(w, h)];
}

void platformSetResizable(webview_t wv, bool on) {
  NSWindow* win = getNSWindow(wv);
  if (!win) {
    debug::warn("platformSetResizable: no native window");
    return;
  }
  win.styleMask = on ? (win.styleMask | NSWindowStyleMaskResizable)
                     : (win.styleMask & ~NSWindowStyleMaskResizable);
}

}  // namespace coconut::window
