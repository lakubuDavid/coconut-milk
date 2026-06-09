/// macOS lifecycle observers using raw Objective-C runtime.
/// No ObjC syntax — avoids ARC complications from .mm files.
///
/// Events dispatched to BOTH frontend (coconut.on) and Lua (coconut.events).

#include "app.h"
#include "bridge.h"
#include "lifecycle.h"

#include <webview/webview.h>

#include <format>
#include <iostream>

#include <objc/runtime.h>
#include <objc/message.h>

// ObjC runtime typedefs for readability
using id       = struct objc_object*;
using SEL      = struct objc_selector*;
using Class    = struct objc_class*;

// NSRect layout (same on x86_64 and arm64)
struct NSRect {
  double x, y, w, h;
};

namespace coconut::lifecycle {

static App* s_app = nullptr;

/// Forward event to both frontend and Lua.
static void dispatch(const std::string& name, nlohmann::json payload) {
  if (!s_app) return;
  bridge::emitToJS(s_app, name, payload);
  bridge::dispatchEventToLua(s_app, name, payload);
}

/// Retrieve the NSWindow * from the webview.
static id getWindow() {
  if (!s_app || !s_app->webview) return nullptr;
  void* winRaw = webview_get_window(s_app->webview);
  return static_cast<id>(winRaw);
}

// --- ObjC callback implementations (extern "C" for IMP pointers) ---

extern "C" void coconut_onResize(id self, SEL cmd, id notification) {
  (void)self; (void)cmd;

  id window = ((id(*)(id, SEL))objc_msgSend)(notification, sel_registerName("object"));
  if (!window) window = getWindow();
  if (!window) return;

  // NSRect > 16 bytes → uses stret (hidden-pointer) ABI on both arm64 and x86_64.
  NSRect frame{};
  frame = ((NSRect(*)(id, SEL))objc_msgSend_stret)(window, sel_registerName("frame"));

  nlohmann::json payload = {
    {"w", static_cast<int>(frame.w)},
    {"h", static_cast<int>(frame.h)}
  };
  dispatch("resize", payload);
}

extern "C" void coconut_onFocus(id self, SEL cmd, id notification) {
  (void)self; (void)cmd; (void)notification;
  dispatch("focus", {{"active", true}});
}

extern "C" void coconut_onBlur(id self, SEL cmd, id notification) {
  (void)self; (void)cmd; (void)notification;
  dispatch("focus", {{"active", false}});
}

// --- Public platform API ---

void platformRegisterEvents(App* app) {
  if (!app || !app->webview) return;
  s_app = app;

  id win = getWindow();
  if (!win) {
    std::cerr << "[lifecycle] no native window handle\n";
    return;
  }

  Class ncClass = objc_getClass("NSNotificationCenter");
  SEL defaultCenterSel = sel_registerName("defaultCenter");
  id defaultCenter = ((id(*)(id, SEL))objc_msgSend)((id)ncClass, defaultCenterSel);

  auto addObserver = [&](const char* notificationName,
                         IMP callback, SEL callbackSel) {
    Class objClass = objc_getClass("NSObject");
    id observer = ((id(*)(id, SEL))objc_msgSend)(
        ((id(*)(id, SEL))objc_msgSend)((id)objClass, sel_registerName("alloc")),
        sel_registerName("init"));

    Class obsClass = object_getClass(observer);
    class_addMethod(obsClass, callbackSel, callback, "v@:@");

    Class strClass = objc_getClass("NSString");
    id notifName = ((id(*)(id, SEL, const char*))objc_msgSend)(
        (id)strClass, sel_registerName("stringWithUTF8String:"), notificationName);

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

void platformUnregisterEvents() {
  s_app = nullptr;
}

} // namespace coconut::lifecycle
