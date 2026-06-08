/// Cocoa NSWindow lifecycle observers for resize, focus, blur.
/// Uses raw Objective-C runtime (no ObjC syntax, no ARC issues).
///
/// Events are dispatched to BOTH:
///   - The frontend via bridge::emitToJS()  → coconut.on('resize', ...)
///   - Lua via       bridge::dispatchEventToLua() → coconut.events(name, payload, ctx)

#include "lifecycle.h"
#include "app.h"
#include "bridge.h"

#include <webview/webview.h>

#include <format>
#include <iostream>

#include <objc/runtime.h>
#include <objc/message.h>

// ObjC runtime typedefs for readability
using id = struct objc_object*;
using SEL = struct objc_selector*;

namespace coconut::lifecycle {

static coconut::App* s_app = nullptr;

/// Dispatch event to both frontend and Lua.
static void dispatch(const std::string& name, nlohmann::json payload) {
  if (!s_app) return;

  // Frontend: coconut.on('resize', cb)
  bridge::emitToJS(s_app, name, payload);

  // Lua:      coconut.events(name, payload, ctx)
  bridge::dispatchEventToLua(s_app, name, payload);
}

/// Called when the window is resized.
extern "C" void coconut_onResize(id self, SEL cmd, id notification) {
  (void)self; (void)cmd; (void)notification;
  nlohmann::json payload = {{"w", 0}, {"h", 0}};
  dispatch("resize", payload);
}

/// Called when the window becomes key (focused).
extern "C" void coconut_onFocus(id self, SEL cmd, id notification) {
  (void)self; (void)cmd; (void)notification;
  dispatch("focus", {{"active", true}});
}

/// Called when the window resigns key (blurred).
extern "C" void coconut_onBlur(id self, SEL cmd, id notification) {
  (void)self; (void)cmd; (void)notification;
  dispatch("focus", {{"active", false}});
}

void registerEvents(coconut::App* app) {
  if (!app || !app->webview) return;
  s_app = app;

  // Get the native NSWindow handle via webview's C API.
  void* winRaw = webview_get_window(app->webview);
  if (!winRaw) {
    std::cerr << "[lifecycle] no native window handle\n";
    return;
  }
  id win = static_cast<id>(winRaw);

  // Get NSNotificationCenter defaultCenter
  Class ncClass = objc_getClass("NSNotificationCenter");
  SEL defaultCenterSel = sel_registerName("defaultCenter");
  id defaultCenter = ((id(*)(id, SEL))objc_msgSend)((id)ncClass, defaultCenterSel);

  // Helper to register a notification observer using the selector-based API.
  auto addObserver = [&](const char* notificationName,
                         IMP callback,
                         SEL callbackSel) {
    // Create a plain NSObject as the observer.
    Class objClass = objc_getClass("NSObject");
    SEL allocSel = sel_registerName("alloc");
    SEL initSel = sel_registerName("init");
    id observer = ((id(*)(id, SEL))objc_msgSend)(
        ((id(*)(id, SEL))objc_msgSend)((id)objClass, allocSel), initSel);

    // Add the callback method to the observer's class.
    Class obsClass = object_getClass(observer);
    class_addMethod(obsClass, callbackSel, callback, "v@:@");

    // Create NSString for the notification name.
    Class strClass = objc_getClass("NSString");
    SEL utf8Sel = sel_registerName("stringWithUTF8String:");
    id notifName = ((id(*)(id, SEL, const char*))objc_msgSend)(
        (id)strClass, utf8Sel, notificationName);

    // Register the observer.
    SEL addSel = sel_registerName("addObserver:selector:name:object:");
    ((void(*)(id, SEL, id, SEL, id, id))objc_msgSend)(
        defaultCenter, addSel, observer, callbackSel, notifName, win);
  };

  addObserver("NSWindowDidResizeNotification",
              (IMP)coconut_onResize, sel_registerName("onResize:"));
  addObserver("NSWindowDidBecomeKeyNotification",
              (IMP)coconut_onFocus, sel_registerName("onFocus:"));
  addObserver("NSWindowDidResignKeyNotification",
              (IMP)coconut_onBlur, sel_registerName("onBlur:"));

  std::cerr << "[lifecycle] registered resize/focus/blur observers\n";
}

void unregisterEvents() {
  // Prevent any further dispatches during teardown.
  // The ObjC observer objects will be released naturally when the
  // process exits — no need to explicitly remove them.
  s_app = nullptr;
}

} // namespace coconut::lifecycle
