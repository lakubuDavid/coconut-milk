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
#import <objc/message.h>
#import <WebKit/WebKit.h>

namespace coconut::platform {

// ── Root directory for resolving coconut:// paths ──────────────────
static std::string g_root_dir;
static Class g_handler_class = nil;

/// Store the root directory (called from installSchemeHandlerHook).
void setSchemeHandlerRoot(const std::string& root_dir) {
  g_root_dir = root_dir;
}

// ── Helper: typed objc_msgSend wrappers ───────────────────────────
// Modern Xcode declares objc_msgSend as returning void. Use explicit
// function pointer casts for methods that return ObjC objects.

static id msgSend_id(id self, SEL op, ...) {
  auto fn = (id (*)(id, SEL, ...))objc_msgSend;
  return fn(self, op);
}

static void msgSend_void(id self, SEL op, ...) {
  auto fn = (void (*)(id, SEL, ...))objc_msgSend;
  fn(self, op);
}

/// Send a message that returns const char* (e.g. UTF8String).
static const char* msgSend_cstr(id self, SEL op) {
  auto fn = (const char* (*)(id, SEL))objc_msgSend;
  return fn(self, op);
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

/// Helper: create an NSString from a C string without ARC issues.
static id nsstr(const char* cstr) {
  SEL alloc = sel_getUid("alloc");
  SEL initWithUTF8 = sel_getUid("initWithUTF8String:");
  id cls = (id)objc_getClass("NSString");
  id str = msgSend_id(cls, alloc);
  return msgSend_id(str, initWithUTF8, cstr);
}

/// Helper: create an NSHTTPURLResponse.
static id makeResponse(id urlObj, NSInteger statusCode) {
  id cls = (id)objc_getClass("NSHTTPURLResponse");
  SEL alloc = sel_getUid("alloc");
  SEL init = sel_getUid("initWithURL:statusCode:HTTPVersion:headerFields:");
  id resp = msgSend_id(cls, alloc);
  return msgSend_id(resp, init, urlObj, statusCode,
                     nsstr("HTTP/1.1"), nil);
}

/// webView:startURLSchemeTask: — reads file and responds.
static id startURLSchemeTask(id self, SEL _cmd, id webView, id task) {
  (void)self;
  (void)_cmd;
  (void)webView;

  @autoreleasepool {
    id request = msgSend_id(task, sel_getUid("request"));
    id urlObj = msgSend_id(request, sel_getUid("URL"));
    id urlStr = msgSend_id(urlObj, sel_getUid("absoluteString"));

    const char* urlCStr = msgSend_cstr(urlStr, sel_getUid("UTF8String"));
    if (!urlCStr) {
      debug::error("scheme_handler: nil URL");
      id resp = makeResponse(urlObj, 404);
      msgSend_void(task, sel_getUid("didReceiveResponse:"), resp);
      msgSend_void(task, sel_getUid("didFinish"));
      return nil;
    }

    std::string url(urlCStr);
    std::string relPath = stripScheme(url);
    std::string filePath = fs::resolve(g_root_dir, relPath);

    debug::info(std::format("scheme_handler: GET {} -> {}", url, filePath));

    auto result = fs::readBytes(filePath);
    if (!result) {
      debug::warn(std::format("scheme_handler: 404 {}", filePath));
      id resp = makeResponse(urlObj, 404);
      msgSend_void(task, sel_getUid("didReceiveResponse:"), resp);
      msgSend_void(task, sel_getUid("didFinish"));
      return nil;
    }

    auto& data = *result;
    auto dot = filePath.rfind('.');
    std::string mime = "application/octet-stream";
    if (dot != std::string::npos) {
      mime = mimeTypeForExtension(filePath.substr(dot + 1));
    }

    // Build NSDictionary header: { @"Content-Type": mime_str }
    id contentTypeKey = nsstr("Content-Type");
    id contentTypeVal = nsstr(mime.c_str());
    id keys = msgSend_id(
        msgSend_id((id)objc_getClass("NSArray"), sel_getUid("alloc")),
        sel_getUid("initWithObjects:count:"),
        &contentTypeKey, (NSUInteger)1);
    id values = msgSend_id(
        msgSend_id((id)objc_getClass("NSArray"), sel_getUid("alloc")),
        sel_getUid("initWithObjects:count:"),
        &contentTypeVal, (NSUInteger)1);

    id headers = msgSend_id(
        msgSend_id((id)objc_getClass("NSDictionary"), sel_getUid("alloc")),
        sel_getUid("initWithObjects:forKeys:count:"),
        values, keys, (NSUInteger)1);

    // Response with 200
    id response = msgSend_id(
        msgSend_id((id)objc_getClass("NSHTTPURLResponse"),
                   sel_getUid("alloc")),
        sel_getUid("initWithURL:statusCode:HTTPVersion:headerFields:"),
        urlObj, (NSInteger)200, nsstr("HTTP/1.1"), headers);

    msgSend_void(task, sel_getUid("didReceiveResponse:"), response);

    // Data
    id nsData = msgSend_id(
        msgSend_id((id)objc_getClass("NSData"), sel_getUid("alloc")),
        sel_getUid("initWithBytes:length:"),
        data.data(), (NSUInteger)data.size());

    msgSend_void(task, sel_getUid("didReceiveData:"), nsData);
    msgSend_void(task, sel_getUid("didFinish"));
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

  id handler = msgSend_id(
      msgSend_id((id)g_handler_class, sel_getUid("alloc")),
      sel_getUid("init"));

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

  id schemeStr = nsstr("coconut");
  msgSend_void(config, setHandlerSel, handler, schemeStr);

  debug::info("scheme_handler: registered coconut:// scheme handler");
}

} // namespace coconut::platform

#endif // __APPLE__
