/// macOS NSEvent keyDown monitor — pure ObjC, no C++ headers.
///
/// Uses a C callback to dispatch events to the C++ layer, avoiding ARC
/// conflicts with the webview library headers.

#import <Cocoa/Cocoa.h>

#include "keyboard.h"

namespace coconut::platform {

static id s_keyMonitor = nil;
static KeyEventCallback s_callback = nullptr;
static void* s_userdata = nullptr;

// ── Helper: normalize NSEvent → combo string ─────────────────────────────

static NSString* normalizeCombo(NSEvent* event) {
  NSEventModifierFlags flags = event.modifierFlags;
  NSString* chars = event.charactersIgnoringModifiers;
  if (!chars || chars.length == 0) return nil;

  NSString* key = [chars lowercaseString];

  // Map special keys via function-key Unicode ranges
  static NSDictionary* specialMap = nil;
  static dispatch_once_t once;
  dispatch_once(&once, ^{
    specialMap = @{
      @"\x1b": @"escape",
      @"\t":   @"tab",
      @"\n":   @"enter",
      @"\r":   @"enter",
      @" ":    @"space",
      @"\x7f": @"backspace",
      [NSString stringWithFormat:@"%C", (unichar)NSUpArrowFunctionKey]:    @"up",
      [NSString stringWithFormat:@"%C", (unichar)NSDownArrowFunctionKey]:  @"down",
      [NSString stringWithFormat:@"%C", (unichar)NSLeftArrowFunctionKey]:  @"left",
      [NSString stringWithFormat:@"%C", (unichar)NSRightArrowFunctionKey]: @"right",
      [NSString stringWithFormat:@"%C", (unichar)NSF1FunctionKey]:  @"f1",
      [NSString stringWithFormat:@"%C", (unichar)NSF2FunctionKey]:  @"f2",
      [NSString stringWithFormat:@"%C", (unichar)NSF3FunctionKey]:  @"f3",
      [NSString stringWithFormat:@"%C", (unichar)NSF4FunctionKey]:  @"f4",
      [NSString stringWithFormat:@"%C", (unichar)NSF5FunctionKey]:  @"f5",
      [NSString stringWithFormat:@"%C", (unichar)NSF6FunctionKey]:  @"f6",
      [NSString stringWithFormat:@"%C", (unichar)NSF7FunctionKey]:  @"f7",
      [NSString stringWithFormat:@"%C", (unichar)NSF8FunctionKey]:  @"f8",
      [NSString stringWithFormat:@"%C", (unichar)NSF9FunctionKey]:  @"f9",
      [NSString stringWithFormat:@"%C", (unichar)NSF10FunctionKey]: @"f10",
      [NSString stringWithFormat:@"%C", (unichar)NSF11FunctionKey]: @"f11",
      [NSString stringWithFormat:@"%C", (unichar)NSF12FunctionKey]: @"f12",
    };
  });

  NSString* mappedKey = specialMap[key];
  if (mappedKey) key = mappedKey;

  NSMutableArray* parts = [NSMutableArray array];
  if (flags & NSEventModifierFlagCommand)  [parts addObject:@"mod"];
  if (flags & NSEventModifierFlagControl)  [parts addObject:@"ctrl"];
  if (flags & NSEventModifierFlagOption)   [parts addObject:@"alt"];
  if (flags & NSEventModifierFlagShift)    [parts addObject:@"shift"];
  [parts addObject:key];

  return [parts componentsJoinedByString:@"+"];
}

// ── Platform-level combo detection ───────────────────────────────────────

static bool isPlatformCombo(NSString* combo) {
  if ([combo isEqualToString:@"mod+h"]) return true;
  if ([combo isEqualToString:@"mod+m"]) return true;
  if ([combo isEqualToString:@"mod+`"]) return true;
  if ([combo isEqualToString:@"mod+tab"]) return true;
  if ([combo isEqualToString:@"mod+shift+tab"]) return true;
  if ([combo isEqualToString:@"mod+space"]) return true;
  return false;
}

// ── Public API ────────────────────────────────────────────────────────────

void registerKeyboardMonitor(void* app_ptr, KeyEventCallback cb, void* userdata) {
  if (s_keyMonitor) return;  // already registered

  s_callback = cb;
  s_userdata = userdata;

  s_keyMonitor = [NSEvent addLocalMonitorForEventsMatchingMask:NSEventMaskKeyDown
      handler:^id (NSEvent* event) {
        NSString* comboStr = normalizeCombo(event);
        if (!comboStr) return event;

        std::string combo([comboStr UTF8String]);
        NSLog(@"[coconut.keyboard] keyDown: combo=%s platform=%d", combo.c_str(), isPlatformCombo(comboStr));

        // 1. Platform-level combos — always consume
        if (isPlatformCombo(comboStr)) {
          NSLog(@"[coconut.keyboard] consumed platform combo: %s", combo.c_str());
          return nil;
        }

        // 2. Forward to C++ callback for keybind registry check
        bool handled = false;
        bool consumed = false;
        if (s_callback) {
          consumed = s_callback(combo, &handled, s_userdata);
          NSLog(@"[coconut.keyboard] callback returned: consumed=%d handled=%d", consumed, handled);
        }

        if (consumed) return nil;
        return event;
      }];

  if (s_keyMonitor) {
    NSLog(@"[coconut.keyboard] NSEvent keyDown monitor registered");
  } else {
    NSLog(@"[coconut.keyboard] FAILED to register NSEvent keyDown monitor");
  }
}

void unregisterKeyboardMonitor() {
  if (s_keyMonitor) {
    [NSEvent removeMonitor:s_keyMonitor];
    s_keyMonitor = nil;
    s_callback = nullptr;
    s_userdata = nullptr;
    NSLog(@"[coconut.keyboard] NSEvent keyDown monitor removed");
  }
}

} // namespace coconut::platform
