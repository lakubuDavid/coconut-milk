#include "platform/darwin/window.h"

#include "config.h"
#include "debug.h"
#include "webview/api.h"

#include <objc/message.h>
#include <objc/runtime.h>

// Cocoa type alias (NSWindowStyleMask). Note: BOOL is provided by <objc/objc.h>.
typedef unsigned long NSUInteger;

namespace coconut::window {

void platformApplyWindowStyle(webview_t wv, Config* cfg) {
  if (wv == nullptr || cfg == nullptr) return;

  using id = struct objc_object*;
  using SEL = struct objc_selector*;
  using Class = struct objc_class*;

  id win = (id)webview_get_window(wv);
  if (!win) {
    debug::warn("platformApplyWindowStyle: no native window handle");
    return;
  }

  // ── Frameless ────────────────────────────────────────────────────────
  if (cfg->frameless) {
    // Use the modern Cocoa pattern for frameless windows:
    //   1. Remove .titled from styleMask (hides titlebar + traffic lights)
    //   2. Keep other masks (resizable, closable, miniaturizable) so they work
    //      programmatically even though the buttons are hidden
    //   3. Set isMovableByWindowBackground for HTML drag handling
    //
    // Read current style mask and remove the Titled bit (1 << 0).
    NSUInteger mask = ((NSUInteger(*)(id, SEL))objc_msgSend)(
        win, sel_registerName("styleMask"));

    mask &= ~(1UL << 0);  // Remove NSWindowStyleMaskTitled

    ((void(*)(id, SEL, NSUInteger))objc_msgSend)(
        win, sel_registerName("setStyleMask:"), mask);

    // Allow dragging the window from webview content (the HTML titlebar
    // handles grab_start / move / grab_end events → ctx.window:move()).
    ((void(*)(id, SEL, BOOL))objc_msgSend)(
        win, sel_registerName("setMovableByWindowBackground:"), YES);

    // Borderless windows may lose the drop shadow; restore it.
    ((void(*)(id, SEL, BOOL))objc_msgSend)(
        win, sel_registerName("setHasShadow:"), YES);

    debug::info("platformApplyWindowStyle: frameless (borderless)");
  }

  // ── Transparency ─────────────────────────────────────────────────────
  if (cfg->transparent) {
    // NSWindow setOpaque:NO — allows transparency
    ((void(*)(id, SEL, BOOL))objc_msgSend)(
        win, sel_registerName("setOpaque:"), NO);

    // Set clear background color
    Class colorClass = objc_getClass("NSColor");
    SEL clearSel = sel_registerName("clearColor");
    id clearColor = ((id(*)(id, SEL))objc_msgSend)((id)colorClass, clearSel);
    ((void(*)(id, SEL, id))objc_msgSend)(
        win, sel_registerName("setBackgroundColor:"), clearColor);

    // Disable window shadow for transparent windows
    ((void(*)(id, SEL, BOOL))objc_msgSend)(
        win, sel_registerName("setHasShadow:"), NO);

    // Inject a frontend script to add the transparent-window CSS class
    // to the body element after the DOM is ready.
    if (wv) {
      webview_init(wv,
          "document.addEventListener('DOMContentLoaded', function(){"
          "  if(document.body) document.body.classList.add('transparent-window');"
          "});");
    }

    debug::info("platformApplyWindowStyle: transparent");
  }
}

} // namespace coconut::window
