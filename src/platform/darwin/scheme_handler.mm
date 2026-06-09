/// Coconut `coconut://` URL scheme handler for macOS (WKWebView).
///
/// Implements a WKURLSchemeHandler using raw ObjC runtime.
/// Compatible with ARC (no manual retain/release).

#if defined(__APPLE__)

#include "debug.h"
#include "fs.h"

#include <cstdint>
#include <format>
#include <string>
#include <vector>

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

static const char* mimeTypeForExtension(const std::string& ext) {
  if (ext == "css")  return "text/css";
  if (ext == "js")   return "application/javascript";
  if (ext == "html") return "text/html";
  if (ext == "htm")  return "text/html";
  if (ext == "json") return "application/json";
  if (ext == "png")  return "image/png";
  if (ext == "jpg" || ext == "jpeg") return "image/jpeg";
  if (ext == "gif")  return "image/gif";
  if (ext == "svg")  return "image/svg+xml";
  if (ext == "ico")  return "image/x-icon";
  if (ext == "woff") return "font/woff";
  if (ext == "woff2") return "font/woff2";
  if (ext == "ttf")  return "font/ttf";
  if (ext == "map")  return "application/json";
  return "application/octet-stream";
}

static std::string stripScheme(const std::string& url) {
  const char* prefix = "coconut://";
  if (url.compare(0, 10, prefix) == 0) {
    return url.substr(10);
  }
  return url;
}

/// webView:startURLSchemeTask: — reads file and responds.
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
      debug::error("scheme_handler: nil URL");
      NSHTTPURLResponse* resp = [[NSHTTPURLResponse alloc]
          initWithURL:urlObj statusCode:404
          HTTPVersion:@"HTTP/1.1" headerFields:nil];
      [taskObj didReceiveResponse:resp];
      [taskObj didFinish];
      return nil;
    }

    std::string url(urlCStr);
    std::string relPath = stripScheme(url);
    std::string filePath = fs::resolve(g_root_dir, relPath);

    debug::info(std::format("scheme_handler: GET {} -> {}", url, filePath));

    auto result = fs::readBytes(filePath);
    if (!result) {
      debug::warn(std::format("scheme_handler: 404 {}", filePath));
      NSHTTPURLResponse* resp = [[NSHTTPURLResponse alloc]
          initWithURL:urlObj statusCode:404
          HTTPVersion:@"HTTP/1.1" headerFields:nil];
      [taskObj didReceiveResponse:resp];
      [taskObj didFinish];
      return nil;
    }

    auto& data = *result;
    auto dot = filePath.rfind('.');
    std::string mime = "application/octet-stream";
    if (dot != std::string::npos) {
      mime = mimeTypeForExtension(filePath.substr(dot + 1));
    }

    // Build NSDictionary header: { @"Content-Type": mime_str }
    NSDictionary* headers = @{
      @"Content-Type": [NSString stringWithUTF8String:mime.c_str()]
    };

    // Response with 200
    NSHTTPURLResponse* response = [[NSHTTPURLResponse alloc]
        initWithURL:urlObj statusCode:200
        HTTPVersion:@"HTTP/1.1" headerFields:headers];

    [taskObj didReceiveResponse:response];

    // Data
    NSData* nsData = [[NSData alloc]
        initWithBytes:data.data() length:(NSUInteger)data.size()];

    [taskObj didReceiveData:nsData];
    [taskObj didFinish];
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

  // Use standard ObjC syntax for alloc/init so ARC handles memory properly.
  id handler = [[(id)g_handler_class alloc] init];

  if (!handler) {
    debug::error("scheme_handler: failed to create handler instance");
    return;
  }

  SEL setHandlerSel = sel_getUid("setURLSchemeHandler:forURLScheme:");
  // Use the runtime function instead of msgSend to avoid casting SEL to id.
  if (!class_respondsToSelector(object_getClass(config), setHandlerSel)) {
    debug::error("scheme_handler: config doesn't respond to selector");
    return;
  }

  [(WKWebViewConfiguration*)config setURLSchemeHandler:handler forURLScheme:@"coconut"];

  debug::info("scheme_handler: registered coconut:// scheme handler");
}

} // namespace coconut::platform

#endif // __APPLE__
