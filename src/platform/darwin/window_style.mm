/// macOS-native window style implementation (frameless, transparent).
///
/// Using .mm file ensures correct ObjC++ calling conventions on arm64.

#import <Cocoa/Cocoa.h>

#include "config.h"
#include "debug.h"
#include "platform/darwin/window.h"

namespace coconut::window {

/// Recursively log all subviews in a view hierarchy.
static void logAllSubviews(NSView* view, int depth) {
  if (!view) return;
  
  NSString* className = NSStringFromClass([view class]);
  std::string indent(depth * 2, ' ');
  debug::info(indent + "view: " + std::string([className UTF8String]) + " tag=" + std::to_string(view.tag));
  
  for (NSView* subview in view.subviews) {
    logAllSubviews(subview, depth + 1);
  }
}

/// Recursively hide NSTitlebarView and NSTextField (title text).
static void hideTitlebarElements(NSView* view) {
  if (!view) return;
  
  for (NSView* subview in view.subviews) {
    NSString* className = NSStringFromClass([subview class]);
    
    // Hide NSTitlebarView (contains traffic lights)
    if ([className isEqualToString:@"NSTitlebarView"]) {
      debug::info("found NSTitlebarView, hiding it");
      subview.hidden = YES;
    }
    // Hide title text field
    if ([subview isKindOfClass:[NSTextField class]]) {
      debug::info("found NSTextField (title), hiding it");
      subview.hidden = YES;
    }
    // Recurse
    hideTitlebarElements(subview);
  }
}

/// Hide traffic lights and title by traversing the view hierarchy.
/// The webview-created window doesn't have standardWindowButton:, so
/// we need to find the buttons through the titlebar view hierarchy.
static void hideTrafficLights(NSWindow* win) {
  debug::info("hiding traffic lights via titlebar view hierarchy...");
  
  NSView* contentView = win.contentView;
  if (!contentView) {
    debug::warn("no contentView found");
    return;
  }
  
  // Navigate: contentView -> NSThemeFrame -> NSTitlebarContainerView -> NSTitlebarView
  NSView* themeFrame = contentView.superview;
  if (!themeFrame) {
    debug::warn("no NSThemeFrame found");
    return;
  }
  
  debug::info("found NSThemeFrame, logging hierarchy...");
  logAllSubviews(themeFrame, 0);
  
  // Recursively hide titlebar elements (NSTitlebarView contains traffic lights)
  hideTitlebarElements(themeFrame);
  
  debug::info("traffic lights hidden");
}

/// Add an invisible toolbar to help with traffic light alignment.
/// The toolbar makes the window treat the traffic light buttons properly.
static void addInvisibleToolbar(NSWindow* win) {
  debug::info("adding invisible toolbar...");
  
  NSToolbar* toolbar = [[NSToolbar alloc] initWithIdentifier:@"coconut-invisible-toolbar"];
  toolbar.displayMode = NSToolbarDisplayModeIconOnly;
  toolbar.allowsUserCustomization = NO;
  toolbar.autosavesConfiguration = NO;
  
  win.toolbar = toolbar;
  win.toolbarStyle = NSWindowToolbarStyleUnified;
  
  debug::info("invisible toolbar added");
}

void platformApplyWindowStyle(webview_t wv, Config* cfg) {
  if (wv == nullptr || cfg == nullptr) return;

  NSWindow* win = (__bridge NSWindow*)webview_get_window(wv);
  if (!win) {
    debug::warn("platformApplyWindowStyle: no native window handle");
    return;
  }

  // ── Frameless ────────────────────────────────────────────────────────
  if (cfg->frameless) {
    debug::info("applying frameless style...");
    
    NSWindowStyleMask mask = win.styleMask;
    debug::info("frameless: before styleMask=" + std::to_string((NSUInteger)mask));

    // Add full-size content view mask (allows content to extend under titlebar)
    // This removes the top gap - content starts at window frame (0,0)
    mask |= NSWindowStyleMaskFullSizeContentView;
    
    // Set titlebar properties to hide it
    win.titlebarAppearsTransparent = YES;
    win.titleVisibility = (NSWindowTitleVisibility)1;  // hidden
    
    win.styleMask = mask;

    // Set background color to avoid grey/gradient appearance
    // Use a dark color similar to typical app backgrounds
    win.backgroundColor = [NSColor windowBackgroundColor];
    
    // Make window opaque (content covers entire window)
    win.opaque = YES;
    
    // Add invisible toolbar to help with traffic light alignment
    addInvisibleToolbar(win);
    
    // Hide traffic lights via view hierarchy traversal
    hideTrafficLights(win);

    // Force window to refresh
    [win orderOut:nil];
    [win makeKeyAndOrderFront:nil];
    [win display];

    debug::info("frameless: after styleMask=" + std::to_string((NSUInteger)win.styleMask));

    // Enable drag-from-content for HTML titlebar handling.
    win.movableByWindowBackground = YES;
    // Restore drop shadow.
    win.hasShadow = YES;

    debug::info("platformApplyWindowStyle: frameless (NSFullSizeContentView)");
  }

  // ── Transparency ─────────────────────────────────────────────────────
  if (cfg->transparent) {
    win.opaque = NO;
    win.backgroundColor = [NSColor clearColor];
    win.hasShadow = NO;  // No shadow for fully transparent windows

    debug::info("platformApplyWindowStyle: transparent");
  }
}

} // namespace coconut::window