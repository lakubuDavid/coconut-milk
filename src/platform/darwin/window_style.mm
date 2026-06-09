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

/// Recursively hide all NSButton subviews in a view hierarchy.
static void hideAllButtonsInView(NSView* view) {
  if (!view) return;
  
  for (NSView* subview in view.subviews) {
    if ([subview isKindOfClass:[NSButton class]]) {
      NSButton* btn = (NSButton*)subview;
      NSString* title = btn.title;
      debug::info("hiding button: " + std::string([title UTF8String]) + " tag=" + std::to_string(btn.tag));
      btn.hidden = YES;
    }
    // Recurse into subview's subviews
    hideAllButtonsInView(subview);
  }
}

void platformApplyWindowStyle(webview_t wv, Config* cfg) {
  if (wv == nullptr || cfg == nullptr) return;

  NSWindow* win = (__bridge NSWindow*)webview_get_window(wv);
  if (!win) {
    debug::warn("platformApplyWindowStyle: no native window handle");
    return;
  }

  // ── Hide traffic lights ───────────────────────────────────────────────
  // The webview-created window may not have standardWindowButton:, so
  // we access the buttons through the titlebar view hierarchy.
  debug::info("hiding traffic lights via titlebar view...");
  
  NSView* contentView = win.contentView;
  NSView* titlebarView = contentView.superview;
  
  debug::info("contentView = " + std::to_string((NSInteger)contentView));
  debug::info("titlebarView = " + std::to_string((NSInteger)titlebarView));
  
  if (titlebarView) {
    debug::info("titlebarView exists, logging full hierarchy...");
    
    // Log all views to find where traffic lights are
    logAllSubviews(titlebarView, 0);
    
    // Recursively hide titlebar elements
    hideTitlebarElements(titlebarView);
    
    debug::info("traffic lights hidden");
  } else {
    debug::warn("no titlebarView found");
  }

  // ── Frameless ────────────────────────────────────────────────────────
  if (cfg->frameless) {
    // Remove Titled bit from styleMask — hides titlebar.
    // Keep other bits (resizable, closable, miniaturizable) for programmatic use.
    NSWindowStyleMask mask = win.styleMask;
    debug::info("frameless: before styleMask=" + std::to_string((NSUInteger)mask));

    mask &= ~NSWindowStyleMaskTitled;
    win.styleMask = mask;

    // Force the window to recalculate its layout after style change.
    [win setFrame:win.frame display:YES animate:NO];

    debug::info("frameless: after styleMask=" + std::to_string((NSUInteger)win.styleMask));

    // Enable drag-from-content for HTML titlebar handling.
    win.movableByWindowBackground = YES;

    // Restore drop shadow (borderless removes it by default).
    win.hasShadow = YES;

    debug::info("platformApplyWindowStyle: frameless");
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