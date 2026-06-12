/// Coconut `coconut://` URL scheme handler for macOS (WKWebView).
///
/// Thin platform adapter — all routing decisions live in routes::handle().
/// This file just maps RouteResult → WKURLSchemeTask callbacks.

#if defined(__APPLE__)

#include "debug.h"
#include "routes.h"

#include <cstdint>
#include <format>
#include <string>

#import <objc/runtime.h>
#import <WebKit/WebKit.h>

namespace coconut::platform {

// ── Root directory for resolving coconut:// paths ──────────────────
static std::string g_root_dir;
static Class g_handler_class = nil;

/// Store the root directory (called from installSchemeHandlerHook).
void setSchemeHandlerRoot(const std::string& root_dir) {
  g_root_dir = root_dir;
}

// ── WKURLSchemeHandler implementation ────────────────────────────

/// webView:startURLSchemeTask: — delegates to routes::handle().
static id startURLSchemeTask(id self, SEL _cmd, id webView, id task) {
  (void)self;
  (void)_cmd;
  (void)webView;

  @autoreleasepool {
    id<WKURLSchemeTask> taskObj = (id<WKURLSchemeTask>)task;
    NSURLRequest* request = [taskObj request];
    NSURL* urlObj = [request URL];
    NSString* urlStr = [urlObj absoluteString];

    const char* urlCStr = [urlStr UTF8String];
    if (!urlCStr) {
      debug::error("scheme_handler: nil URL, responding 404");
      NSHTTPURLResponse* resp = [[NSHTTPURLResponse alloc]
          initWithURL:urlObj statusCode:404
          HTTPVersion:@"HTTP/1.1" headerFields:nil];
      [taskObj didReceiveResponse:resp];
      [taskObj didFinish];
      return nil;
    }

    std::string url(urlCStr);
    debug::info(std::format("scheme_handler: request '{}'", url));

    // ── Delegate all routing to the platform-agnostic routes module ──
    auto action = routes::handle(url, g_root_dir);

    switch (action.type) {

      case routes::RouteResult::NAVIGATE_VIEW: {
        debug::info(std::format("scheme_handler: navigate to view '{}'", action.view_name));
        NSString* js = [NSString stringWithFormat:
            @"coconut.emit('navigate', {view:'%s'})", action.view_name.c_str()];
        [(WKWebView*)webView evaluateJavaScript:js completionHandler:nil];
        debug::info("scheme_handler: responding 204 (no content)");
        NSHTTPURLResponse* resp = [[NSHTTPURLResponse alloc]
            initWithURL:urlObj statusCode:204
            HTTPVersion:@"HTTP/1.1" headerFields:nil];
        [taskObj didReceiveResponse:resp];
        [taskObj didFinish];
        break;
      }

      case routes::RouteResult::SERVE_FILE: {
        debug::info(std::format("scheme_handler: serve '{}' ({} bytes, {})",
                    action.file_path, action.data.size(), action.mime_type));
        NSDictionary* headers = @{
          @"Content-Type": [NSString stringWithUTF8String:action.mime_type.c_str()]
        };
        NSHTTPURLResponse* response = [[NSHTTPURLResponse alloc]
            initWithURL:urlObj statusCode:200
            HTTPVersion:@"HTTP/1.1" headerFields:headers];
        [taskObj didReceiveResponse:response];
        NSData* nsData = [[NSData alloc]
            initWithBytes:action.data.data() length:(NSUInteger)action.data.size()];
        [taskObj didReceiveData:nsData];
        [taskObj didFinish];
        break;
      }

      case routes::RouteResult::NOT_FOUND:
      default: {
        debug::warn(std::format("scheme_handler: 404 '{}'", url));
        NSHTTPURLResponse* resp = [[NSHTTPURLResponse alloc]
            initWithURL:urlObj statusCode:404
            HTTPVersion:@"HTTP/1.1" headerFields:nil];
        [taskObj didReceiveResponse:resp];
        [taskObj didFinish];
        break;
      }
    }
  }

  return nil;
}

/// webView:stopURLSchemeTask: — no-op.
static id stopURLSchemeTask(id self, SEL _cmd, id webView, id task) {
  (void)self;
  (void)_cmd;
  (void)webView;
  (void)task;
  return nil;
}

/// Register the WKURLSchemeHandler on the given WKWebViewConfiguration.
void registerWKURLSchemeHandler(void* configPtr) {
  id config = (__bridge id)configPtr;
  if (!config) {
    debug::error("scheme_handler: null config");
    return;
  }

  if (!g_handler_class) {
    g_handler_class = objc_allocateClassPair(
        objc_getClass("NSObject"), "CoconutSchemeHandler", 0);
    if (!g_handler_class) {
      debug::error("scheme_handler: failed to allocate class");
      return;
    }

    Protocol* proto = objc_getProtocol("WKURLSchemeHandler");
    if (proto) {
      class_addProtocol(g_handler_class, proto);
    }

    class_addMethod(g_handler_class,
                    sel_getUid("webView:startURLSchemeTask:"),
                    (IMP)startURLSchemeTask, "v@:@@@");
    class_addMethod(g_handler_class,
                    sel_getUid("webView:stopURLSchemeTask:"),
                    (IMP)stopURLSchemeTask, "v@:@@@");

    objc_registerClassPair(g_handler_class);
    debug::info("scheme_handler: created CoconutSchemeHandler class");
  }

  debug::info("scheme_handler: about to alloc handler...");

  id handler = [[(id)g_handler_class alloc] init];

  if (!handler) {
    debug::error("scheme_handler: failed to create handler instance");
    return;
  }

  SEL setHandlerSel = sel_getUid("setURLSchemeHandler:forURLScheme:");
  if (!class_respondsToSelector(object_getClass(config), setHandlerSel)) {
    debug::error("scheme_handler: config doesn't respond to selector");
    return;
  }

  [(WKWebViewConfiguration*)config setURLSchemeHandler:handler forURLScheme:@"coconut"];

  debug::info("scheme_handler: registered coconut:// scheme handler");
}

} // namespace coconut::platform

#endif // __APPLE__
