/// macOS-native window style implementation (frameless, transparent).
///
/// Using .mm file ensures correct ObjC++ calling conventions on arm64.

#import <Cocoa/Cocoa.h>
#import <WebKit/WebKit.h>

#include "config.h"
#include "debug.h"
#include "platform/darwin/window.h"

#include <format>
#include <string>

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
      coconut::debug::info("navDelegate: nil URL, allowing");
      decisionHandler(WKNavigationActionPolicyAllow);
      return;
    }

    NSString* urlStr = [nsURL absoluteString];
    WKNavigationType navType = [navigationAction navigationType];

    // Map navigation type to readable string
    const char* typeStr = "unknown";
    switch (navType) {
      case WKNavigationTypeLinkActivated:    typeStr = "link"; break;
      case WKNavigationTypeFormSubmitted:    typeStr = "form"; break;
      case WKNavigationTypeBackForward:      typeStr = "backfwd"; break;
      case WKNavigationTypeReload:           typeStr = "reload"; break;
      case WKNavigationTypeFormResubmitted:  typeStr = "formresubmit"; break;
      case WKNavigationTypeOther:            typeStr = "other"; break;
    }

    coconut::debug::info(std::format("navDelegate: type={} url={}", typeStr, [urlStr UTF8String]));

    // Only intercept user-initiated navigations (link clicks, form submits).
    // Always allow sub-resource loads (CSS, JS, images, AJAX, initial page load).
    if (navType != WKNavigationTypeLinkActivated &&
        navType != WKNavigationTypeFormSubmitted) {
      coconut::debug::info("navDelegate: sub-resource or initial load, allowing");
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
      coconut::debug::info(std::format("navDelegate: ALLOW {} (internal scheme)", [urlStr UTF8String]));
      decisionHandler(WKNavigationActionPolicyAllow);
      return;
    }

    // ── External URL: open in system browser ─────────────────────────
    // Cancel the webview navigation and open in the default browser.
    coconut::debug::info(std::format("navDelegate: OPEN IN BROWSER {}", [urlStr UTF8String]));
    [[NSWorkspace sharedWorkspace] openURL:nsURL];
    decisionHandler(WKNavigationActionPolicyCancel);
  }
}

- (void)webView:(WKWebView *)webView didFinishNavigation:(WKNavigation *)navigation {
  coconut::debug::info(std::format("navDelegate: didFinishNavigation for {}",
      [[[webView URL] absoluteString] UTF8String]));
}

- (void)webView:(WKWebView *)webView didFailNavigation:(WKNavigation *)navigation withError:(NSError *)error {
  coconut::debug::info(std::format("navDelegate: didFailNavigation: {}",
      [[error localizedDescription] UTF8String]));
}

- (void)webView:(WKWebView *)webView didFailProvisionalNavigation:(WKNavigation *)navigation withError:(NSError *)error {
  coconut::debug::info(std::format("navDelegate: didFailProvisionalNavigation: {}",
      [[error localizedDescription] UTF8String]));
}

@end

// ═══════════════════════════════════════════════════════════════════════
// C++ implementation inside coconut::window namespace
// ═══════════════════════════════════════════════════════════════════════

namespace coconut::window {

/// Track whether the WKNavigationDelegate has been installed.
static BOOL _navDelegateInstalled = NO;

/// Strong reference to keep the WKNavigationDelegate alive.
/// WKWebView's navigationDelegate property is weak, so without this
/// the delegate would be deallocated by ARC after installNavDelegate returns.
static CoconutNavDelegate* s_strongNavDelegate = nil;

/// Recursively log all subviews in a view hierarchy.
static void logAllSubviews(NSView* view, int depth) {
  if (!view) return;
  
  NSString* className = NSStringFromClass([view class]);
  std::string indent(depth * 2, ' ');
  debug::log(indent + "view: " + std::string([className UTF8String]) + " tag=" + std::to_string(view.tag));
  
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
      debug::log("found NSTitlebarView, hiding it");
      subview.hidden = YES;
    }
    // Hide title text field
    if ([subview isKindOfClass:[NSTextField class]]) {
      debug::log("found NSTextField (title), hiding it");
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
  
  debug::log("found NSThemeFrame, logging hierarchy...");
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
  
  s_strongNavDelegate = [[CoconutNavDelegate alloc] init];
  coconut::debug::info(std::format("installNavDelegate: WKWebView={:#x}",
      reinterpret_cast<uintptr_t>(wkWebView)));
  // Use the direct property setter instead of KVC to avoid issues
  [(WKWebView*)wkWebView setNavigationDelegate:s_strongNavDelegate];
  
  // Verify the delegate was set by reading it back
  id currentDelegate = [(WKWebView*)wkWebView navigationDelegate];
  coconut::debug::info(std::format("installNavDelegate: delegate match={}",
      currentDelegate == s_strongNavDelegate ? "yes" : "no"));
  
  _navDelegateInstalled = YES;
  coconut::debug::info("installNavDelegate: WKNavigationDelegate installed");
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

void platformInstallNavDelegate(webview_t wv) {
  if (!wv) return;
  NSWindow* win = (__bridge NSWindow*)webview_get_window(wv);
  if (!win) {
    debug::warn("platformInstallNavDelegate: no native window");
    return;
  }
  installNavDelegate(win);
}

} // namespace coconut::window