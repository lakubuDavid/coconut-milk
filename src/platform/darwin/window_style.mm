/// macOS-native window style implementation (frameless, transparent).
///
/// Using .mm file ensures correct ObjC++ calling conventions on arm64.

#import <Cocoa/Cocoa.h>

#include "config.h"
#include "debug.h"
#include "platform/darwin/window.h"

namespace coconut::window {

void platformApplyWindowStyle(webview_t wv, Config* cfg) {
  if (wv == nullptr || cfg == nullptr) return;

  NSWindow* win = (__bridge NSWindow*)webview_get_window(wv);
  if (!win) {
    debug::warn("platformApplyWindowStyle: no native window handle");
    return;
  }

  // ── Frameless ────────────────────────────────────────────────────────
  if (cfg->frameless) {
    // Remove Titled bit from styleMask — hides titlebar + traffic lights.
    // Keep other bits (resizable, closable, miniaturizable) for programmatic use.
    NSWindowStyleMask mask = win.styleMask;
    debug::info("frameless: before styleMask=" + std::to_string((NSUInteger)mask));

    mask &= ~NSWindowStyleMaskTitled;
    win.styleMask = mask;

    // Force the window to recalculate its layout after style change.
    // Without this, the titlebar may remain visible even though styleMask is 0.
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