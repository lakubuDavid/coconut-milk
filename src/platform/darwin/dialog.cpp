/// macOS native dialog implementations using raw Objective-C runtime.
/// Covers: NSAlert (message box), NSOpenPanel (file open), NSSavePanel (file save).
///
/// No ObjC syntax — avoids ARC complications.  All calls through
/// objc_msgSend / objc_msgSend_stret.

#include "dialog.h"
#include "../../debug.h"

#include <objc/message.h>
#include <objc/runtime.h>
#include <stdint.h>

// Minimal ObjC type defs for types not available from raw objc/runtime.h
typedef long NSInteger;
typedef unsigned long NSUInteger;

#include <format>
#include <string>
#include <vector>

// ObjC runtime typedefs
using id    = struct objc_object*;
using SEL   = struct objc_selector*;
using Class = struct objc_class*;

namespace coconut::dialog {

// ── Helpers ────────────────────────────────────────────────────────────

/// Return the shared NSApplication (ensures app delegate etc. is set up).
static id sharedApplication() {
  Class appClass = objc_getClass("NSApplication");
  SEL sharedSel = sel_registerName("sharedApplication");
  return ((id(*)(id, SEL))objc_msgSend)((id)appClass, sharedSel);
}

/// Convert a C++ string to an ObjC NSString.
static id nsString(const std::string& s) {
  Class strClass = objc_getClass("NSString");
  SEL sel = sel_registerName("stringWithUTF8String:");
  return ((id(*)(id, SEL, const char*))objc_msgSend)(
      (id)strClass, sel, s.c_str());
}

/// Activate the app (bring dialogs to front).
static void activateApp() {
  id app = sharedApplication();
  SEL activateSel = sel_registerName("activateIgnoringOtherApps:");
  ((void(*)(id, SEL, BOOL))objc_msgSend)(app, activateSel, (BOOL)YES);
}

// ── Message box (NSAlert) ───────────────────────────────────────────────

Result platformMessageBox(const std::string& title,
                          const std::string& message,
                          const std::string& kind) {
  Result result{};

  Class alertClass = objc_getClass("NSAlert");
  SEL allocSel = sel_registerName("alloc");
  SEL initSel = sel_registerName("init");

  id alert = ((id(*)(id, SEL))objc_msgSend)(
      ((id(*)(id, SEL))objc_msgSend)((id)alertClass, allocSel), initSel);

  // Set message text
  SEL setMsgSel = sel_registerName("setMessageText:");
  ((void(*)(id, SEL, id))objc_msgSend)(alert, setMsgSel, nsString(title));

  // Set informative text
  SEL setInfoSel = sel_registerName("setInformativeText:");
  ((void(*)(id, SEL, id))objc_msgSend)(alert, setInfoSel, nsString(message));

  // Set alert style based on kind
  SEL setStyleSel = sel_registerName("setAlertStyle:");
  if (kind == "error") {
    // NSAlertStyleCritical = 2
    ((void(*)(id, SEL, NSInteger))objc_msgSend)(alert, setStyleSel, (NSInteger)2);
  } else if (kind == "warn") {
    // NSAlertStyleWarning = 1
    ((void(*)(id, SEL, NSInteger))objc_msgSend)(alert, setStyleSel, (NSInteger)1);
  } else {
    // NSAlertStyleInformational = 0 (also "info" and "question")
    ((void(*)(id, SEL, NSInteger))objc_msgSend)(alert, setStyleSel, (NSInteger)0);
  }

  // Add buttons
  SEL addBtnSel = sel_registerName("addButtonWithTitle:");
  if (kind == "question") {
    ((void(*)(id, SEL, id))objc_msgSend)(alert, addBtnSel,
                                          nsString("Yes"));
    ((void(*)(id, SEL, id))objc_msgSend)(alert, addBtnSel,
                                          nsString("No"));
  } else {
    ((void(*)(id, SEL, id))objc_msgSend)(alert, addBtnSel,
                                          nsString("OK"));
  }

  activateApp();

  // Run modal
  SEL runSel = sel_registerName("runModal");
  // NSModalResponse is NSInteger; first button returns NSAlertFirstButtonReturn (= 1000)
  NSInteger response = ((NSInteger(*)(id, SEL))objc_msgSend)(alert, runSel);

  // NSAlertFirstButtonReturn = 1000
  result.confirmed = (response == 1000);

  debug::log(std::format("dialog::messageBox('{}') → confirmed={}", title, result.confirmed));
  return result;
}

// ── Open file / directory (NSOpenPanel) ─────────────────────────────────

Result platformOpenFile(const std::string& title,
                        const std::vector<Filter>& filters,
                        bool multi,
                        bool chooseDir) {
  Result result{};

  Class panelClass = objc_getClass("NSOpenPanel");
  SEL allocSel = sel_registerName("alloc");
  SEL initSel = sel_registerName("init");
  SEL openPanelSel = sel_registerName("openPanel");

  id panel = ((id(*)(id, SEL))objc_msgSend)(
      ((id(*)(id, SEL))objc_msgSend)((id)panelClass, openPanelSel), initSel);

  // Title
  SEL setTitleSel = sel_registerName("setTitle:");
  ((void(*)(id, SEL, id))objc_msgSend)(panel, setTitleSel, nsString(title));

  // Can choose files
  SEL setFilesSel = sel_registerName("setCanChooseFiles:");
  ((void(*)(id, SEL, BOOL))objc_msgSend)(panel, setFilesSel, (BOOL)YES);

  // Can choose directories
  SEL setDirsSel = sel_registerName("setCanChooseDirectories:");
  ((void(*)(id, SEL, BOOL))objc_msgSend)(panel, setDirsSel,
      chooseDir ? (BOOL)YES : (BOOL)NO);

  // Multi selection
  SEL setMultiSel = sel_registerName("setAllowsMultipleSelection:");
  ((void(*)(id, SEL, BOOL))objc_msgSend)(panel, setMultiSel, multi ? (BOOL)YES : (BOOL)NO);

  // Don't show file-types filter when picking directories — it's confusing
  if (!chooseDir && !filters.empty()) {
    SEL setAllowedSel = sel_registerName("setAllowedFileTypes:");
    id typesArray = ((id(*)(id, SEL))objc_msgSend)(
        ((id(*)(id, SEL))objc_msgSend)(
            (id)objc_getClass("NSMutableArray"), sel_registerName("alloc")),
        sel_registerName("init"));

    for (const auto& filter : filters) {
      for (const auto& pattern : filter.patterns) {
        std::string ext = pattern;
        if (ext.size() > 2 && ext.substr(0, 2) == "*.") {
          ext = ext.substr(2);
        }
        SEL addObjSel = sel_registerName("addObject:");
        ((void(*)(id, SEL, id))objc_msgSend)(typesArray, addObjSel,
                                              nsString(ext));
      }
    }
    ((void(*)(id, SEL, id))objc_msgSend)(panel, setAllowedSel, typesArray);
  }

  activateApp();

  // Run modal
  SEL runSel = sel_registerName("runModal");
  NSInteger response = ((NSInteger(*)(id, SEL))objc_msgSend)(panel, runSel);

  if (response == 1) { // NSModalResponseOK = 1
    SEL urlsSel = sel_registerName("URLs");
    id urls = ((id(*)(id, SEL))objc_msgSend)(panel, urlsSel);

    // URLs is an NSArray of NSURL
    SEL countSel = sel_registerName("count");
    NSUInteger count = ((NSUInteger(*)(id, SEL))objc_msgSend)(urls, countSel);

    SEL objAtSel = sel_registerName("objectAtIndex:");
    for (NSUInteger i = 0; i < count; ++i) {
      id url = ((id(*)(id, SEL, NSUInteger))objc_msgSend)(urls, objAtSel, i);
      SEL pathSel = sel_registerName("path");
      const char* cpath = ((const char*(*)(id, SEL))objc_msgSend)(url, pathSel);
      if (cpath) {
        result.paths.push_back(cpath);
      }
    }

    if (!result.paths.empty()) {
      result.path = result.paths[0];
      // Check if the selected path is a directory via NSFileManager
      Class fmClass = objc_getClass("NSFileManager");
      id fm = ((id(*)(id, SEL))objc_msgSend)(
          (id)fmClass, sel_registerName("defaultManager"));
      id nsPath = nsString(result.path);
      SEL fileExistsSel = sel_registerName("fileExistsAtPath:isDirectory:");
      BOOL isDir = NO;
      ((void(*)(id, SEL, id, BOOL*))objc_msgSend)(
          fm, fileExistsSel, nsPath, &isDir);
      result.is_dir = (isDir == YES);
    }
    result.confirmed = true;
  }

  debug::log(std::format("dialog::openFile('{}') → confirmed={}, paths={}, is_dir={}",
                           title, result.confirmed, result.paths.size(), result.is_dir));
  return result;
}

// ── Save file (NSSavePanel) ─────────────────────────────────────────────

Result platformSaveFile(const std::string& title,
                        const std::string& defaultName,
                        const std::vector<Filter>& filters) {
  Result result{};

  Class panelClass = objc_getClass("NSSavePanel");
  SEL allocSel = sel_registerName("alloc");
  SEL initSel = sel_registerName("init");
  SEL savePanelSel = sel_registerName("savePanel");

  id panel = ((id(*)(id, SEL))objc_msgSend)(
      ((id(*)(id, SEL))objc_msgSend)((id)panelClass, savePanelSel), initSel);

  // Title
  SEL setTitleSel = sel_registerName("setTitle:");
  ((void(*)(id, SEL, id))objc_msgSend)(panel, setTitleSel, nsString(title));

  // Default file name
  if (!defaultName.empty()) {
    SEL setNameSel = sel_registerName("setNameFieldStringValue:");
    ((void(*)(id, SEL, id))objc_msgSend)(panel, setNameSel, nsString(defaultName));
  }

  // File filters
  if (!filters.empty()) {
    SEL setAllowedSel = sel_registerName("setAllowedFileTypes:");
    id typesArray = ((id(*)(id, SEL))objc_msgSend)(
        ((id(*)(id, SEL))objc_msgSend)(
            (id)objc_getClass("NSMutableArray"), sel_registerName("alloc")),
        sel_registerName("init"));

    for (const auto& filter : filters) {
      for (const auto& pattern : filter.patterns) {
        std::string ext = pattern;
        if (ext.size() > 2 && ext.substr(0, 2) == "*.") {
          ext = ext.substr(2);
        }
        SEL addObjSel = sel_registerName("addObject:");
        ((void(*)(id, SEL, id))objc_msgSend)(typesArray, addObjSel,
                                              nsString(ext));
      }
    }
    ((void(*)(id, SEL, id))objc_msgSend)(panel, setAllowedSel, typesArray);
  }

  activateApp();

  // Run modal
  SEL runSel = sel_registerName("runModal");
  NSInteger response = ((NSInteger(*)(id, SEL))objc_msgSend)(panel, runSel);

  if (response == 1) { // NSModalResponseOK = 1
    SEL urlSel = sel_registerName("URL");
    id url = ((id(*)(id, SEL))objc_msgSend)(panel, urlSel);
    SEL pathSel = sel_registerName("path");
    const char* cpath = ((const char*(*)(id, SEL))objc_msgSend)(url, pathSel);
    if (cpath) {
      result.path = cpath;
    }
    result.confirmed = true;
  }

  debug::log(std::format("dialog::saveFile('{}') → confirmed={}, path='{}'",
                           title, result.confirmed, result.path));
  return result;
}

} // namespace coconut::dialog
