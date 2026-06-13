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

/// Detect if running inside a .app bundle.
/// Returns a pointer to a static buffer with the Resources path, or NULL.
/// The returned string is valid until the next call (single-threaded use).
const char *coconut_bundle_resource_path() {
  @autoreleasepool {
    NSBundle *bundle = [NSBundle mainBundle];
    NSString *bid = bundle.bundleIdentifier;
    if (bid == nil || [bid length] == 0) {
      return NULL;
    }
    NSString *resPath = bundle.resourcePath;
    if (resPath == nil) {
      return NULL;
    }
    static char buf[4096];
    if (![resPath getCString:buf maxLength:sizeof(buf) encoding:NSUTF8StringEncoding]) {
      return NULL;
    }
    return buf;
  }
}

} // extern "C"
