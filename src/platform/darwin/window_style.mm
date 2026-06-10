/// macOS-native window style implementation (frameless, transparent).
///
/// Using .mm file ensures correct ObjC++ calling conventions on arm64.

#import <Cocoa/Cocoa.h>
#import <WebKit/WebKit.h>

#include "config.h"
#include "debug.h"
#include "platform/darwin/window.h"

// ═══════════════════════════════════════════════════════════════════════
// ObjC classes (file scope — must be outside C++ namespace)
// ═══════════════════════════════════════════════════════════════════════

/// WKNavigationDelegate class that intercepts navigation actions.
/// Non-allowlisted URLs open in the system browser instead of
/// navigating inside the webview.
@interface CoconutNavDelegate : NSObject <WKNavigationDelegate>
@end

@implementation CoconutNavDelegate

- (void)webView:(WKWebView *)webView
decidePolicyForNavigationAction:(WKNavigationAction *)navigationAction
decisionHandler:(void (^)(WKNavigationActionPolicy))decisionHandler {
  @autoreleasepool {
    NSURL* nsURL = [navigationAction.request URL];
    if (!nsURL) {
      decisionHandler(WKNavigationActionPolicyAllow);
      return;
    }

    NSString* scheme = [nsURL scheme];
    NSString* host = [nsURL host];

    // ── Allow list ───────────────────────────────────────────────────
    // Always allow internal protocols and loopback hosts.
    BOOL allow = NO;

    if ([scheme isEqualToString:@"file"])         allow = YES;
    else if ([scheme isEqualToString:@"coconut"]) allow = YES;
    else if ([scheme isEqualToString:@"about"])   allow = YES;
    else if ([scheme isEqualToString:@"data"])    allow = YES;
    else if ([scheme isEqualToString:@"blob"])    allow = YES;
    // Localhost / loopback
    else if (host && ([host isEqualToString:@"localhost"] ||
                      [host isEqualToString:@"127.0.0.1"] ||
                      [host isEqualToString:@"0.0.0.0"] ||
                      [host hasPrefix:@"local."])) {
      allow = YES;
    }

    if (allow) {
      decisionHandler(WKNavigationActionPolicyAllow);
      return;
    }

    // ── External URL: open in system browser ─────────────────────────
    // Cancel the webview navigation and open in the default browser.
    [[NSWorkspace sharedWorkspace] openURL:nsURL];
    decisionHandler(WKNavigationActionPolicyCancel);
  }
}

@end

// ═══════════════════════════════════════════════════════════════════════
// C++ implementation inside coconut::window namespace
// ═══════════════════════════════════════════════════════════════════════

namespace coconut::window {

/// Track whether the WKNavigationDelegate has been installed.
static BOOL _navDelegateInstalled = NO;

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
static void hideTrafficLights(NSWindow* win) {
  debug::info("hiding traffic lights via titlebar view hierarchy...");
  
  NSView* contentView = win.contentView;
  if (!contentView) {
    debug::warn("no contentView found");
    return;
  }
  
  NSView* themeFrame = contentView.superview;
  if (!themeFrame) {
    debug::warn("no NSThemeFrame found");
    return;
  }
  
  debug::info("found NSThemeFrame, logging hierarchy...");
  logAllSubviews(themeFrame, 0);
  hideTitlebarElements(themeFrame);
  debug::info("traffic lights hidden");
}

/// Add an invisible toolbar to help with traffic light alignment.
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

/// Find the WKWebView in the window's view hierarchy by walking subviews.
static id findWKWebView(NSView* view) {
  if (!view) return nil;
  
  NSString* className = NSStringFromClass([view class]);
  if ([className isEqualToString:@"WKWebView"]) {
    return view;
  }
  
  for (NSView* subview in [view subviews]) {
    id found = findWKWebView(subview);
    if (found) return found;
  }
  return nil;
}

/// Install the WKNavigationDelegate on this window's WKWebView.
/// Called once from platformApplyWindowStyle.
void installNavDelegate(NSWindow* win) {
  if (_navDelegateInstalled) return;
  
  NSView* contentView = [win contentView];
  id wkWebView = findWKWebView(contentView);
  if (!wkWebView) {
    debug::warn("installNavDelegate: WKWebView not found");
    return;
  }
  
  CoconutNavDelegate* delegate = [[CoconutNavDelegate alloc] init];
  [wkWebView setValue:delegate forKey:@"navigationDelegate"];
  
  _navDelegateInstalled = YES;
  debug::info("installNavDelegate: WKNavigationDelegate installed");
}

void platformApplyWindowStyle(webview_t wv, Config* cfg) {
  if (wv == nullptr || cfg == nullptr) return;

  NSWindow* win = (__bridge NSWindow*)webview_get_window(wv);
  if (!win) {
    debug::warn("platformApplyWindowStyle: no native window handle");
    return;
  }

  // Install WKNavigationDelegate to intercept external URLs.
  installNavDelegate(win);

  // ── Frameless ────────────────────────────────────────────────────────
  if (cfg->frameless) {
    debug::info("applying frameless style...");
    
    NSWindowStyleMask mask = win.styleMask;
    debug::info("frameless: before styleMask=" + std::to_string((NSUInteger)mask));

    mask |= NSWindowStyleMaskFullSizeContentView;
    
    win.titlebarAppearsTransparent = YES;
    win.titleVisibility = (NSWindowTitleVisibility)1;
    win.styleMask = mask;
    win.backgroundColor = [NSColor windowBackgroundColor];
    win.opaque = YES;
    
    addInvisibleToolbar(win);
    hideTrafficLights(win);

    [win orderOut:nil];
    [win makeKeyAndOrderFront:nil];
    [win display];

    debug::info("frameless: after styleMask=" + std::to_string((NSUInteger)win.styleMask));

    win.movableByWindowBackground = YES;
    win.hasShadow = YES;

    debug::info("platformApplyWindowStyle: frameless (NSFullSizeContentView)");
  }

  // ── Transparency ─────────────────────────────────────────────────────
  if (cfg->transparent) {
    win.opaque = NO;
    win.backgroundColor = [NSColor clearColor];
    win.hasShadow = NO;

    debug::info("platformApplyWindowStyle: transparent");
  }
}

} // namespace coconut::window