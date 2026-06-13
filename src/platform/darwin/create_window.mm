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
/// Only returns a path when the bundle is a real .app wrapper
/// (not a standalone binary with an embedded plist).
const char *coconut_bundle_resource_path() {
  @autoreleasepool {
    NSBundle *bundle = [NSBundle mainBundle];

    // Must have a bundle identifier (from Info.plist)
    NSString *bid = bundle.bundleIdentifier;
    if (bid == nil || [bid length] == 0) {
      return NULL;
    }

    // Must be inside a .app bundle, not a standalone binary
    NSString *bpath = bundle.bundlePath;
    if (bpath == nil || ![bpath hasSuffix:@".app"]) {
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

/// Apply darwin.* config fields to the live NSBundle.
/// Mutates [[NSBundle mainBundle] infoDictionary] in-place.
void coconut_apply_darwin_config(const char *bundle_identifier,
                                  const char *notification_alert_style,
                                  const char **usage_desc_keys,
                                  const char **usage_desc_values,
                                  int usage_desc_count) {
  @autoreleasepool {
    NSBundle *bundle = [NSBundle mainBundle];
    // infoDictionary returns NSDictionary; cast to mutable to set keys.
    NSMutableDictionary *info = (NSMutableDictionary *)[bundle infoDictionary];
    if (!info) return;

    if (bundle_identifier && bundle_identifier[0]) {
      [info setObject:[NSString stringWithUTF8String:bundle_identifier]
                forKey:@"CFBundleIdentifier"];
    }

    if (notification_alert_style && notification_alert_style[0]) {
      [info setObject:[NSString stringWithUTF8String:notification_alert_style]
                forKey:@"NSUserNotificationAlertStyle"];
    }

    for (int i = 0; i < usage_desc_count; i++) {
      if (usage_desc_keys[i] && usage_desc_values[i]) {
        [info setObject:[NSString stringWithUTF8String:usage_desc_values[i]]
                  forKey:[NSString stringWithUTF8String:usage_desc_keys[i]]];
      }
    }
  }
}

} // extern "C"
