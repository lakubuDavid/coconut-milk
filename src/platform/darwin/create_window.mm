/// macOS window creation helpers for frameless windows.
/// These functions can be called from C++ code after being compiled as ObjC++.

#import <Cocoa/Cocoa.h>

extern "C" {

// Helper to create NSWindow (avoiding @autoreleasepool in extern C)
static inline void *create_ns_window_inner(int x, int y, int w, int h,
                                           BOOL frameless) {
  NSRect frame = NSMakeRect(x, y, w, h);

  NSWindowStyleMask style;
  if (frameless) {
    // Use FullSizeContentView with hidden titlebar instead of borderless
    style = NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable |
            NSWindowStyleMaskFullSizeContentView;
  } else {
    style = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
            NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable;
  }

  NSWindow *window =
      [[NSWindow alloc] initWithContentRect:frame
                                  styleMask:style
                                    backing:NSBackingStoreBuffered
                                      defer:NO];
  if (!window)
    return nullptr;

  if (frameless) {
    // Hide the titlebar
    window.titlebarAppearsTransparent = YES;
    window.titleVisibility = (NSWindowTitleVisibility)1; // hidden
    window.movableByWindowBackground = YES;
    window.hasShadow = YES;
  }
  window.titlebarSeparatorStyle = (NSTitlebarSeparatorStyle)2;

  // Use __bridge_retained to transfer ownership to caller
  // This ensures the window is not deallocated when the autoreleasepool drains
  return (__bridge_retained void *)window;
}

/// Create a frameless NSWindow (using fullSizeContentView approach).
/// Returns a void* (the NSWindow*) that can be passed to webview_create().
/// Returns nullptr on failure.
void *coconut_create_frameless_window(int x, int y, int w, int h) {
  @autoreleasepool {
    return create_ns_window_inner(x, y, w, h, YES);
  }
}

/// Create a standard NSWindow (with titlebar).
void *coconut_create_standard_window(int x, int y, int w, int h) {
  @autoreleasepool {
    return create_ns_window_inner(x, y, w, h, NO);
  }
}

} // extern "C"
