#include "window.h"

#include "debug.h"

#include <exception>
#include <expected>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

// ObjC runtime for native window style manipulation
#include <objc/message.h>
#include <objc/runtime.h>

typedef long NSInteger;
typedef unsigned long NSUInteger;

namespace coconut::window {

std::expected<Window *, Error> createWindow(Config *config, webview_t wv) {
  if (wv == nullptr) {
    return std::unexpected(Error{.code = ErrorCode::WebUiError, .message = "Invalid webview handle"});
  }

  return new Window{.configs = config, .views = {}, .webview = wv};
}

void destroyWindow(Window *window) {
  if (window == nullptr) {
    return;
  }

  // Free any heap-allocated views to prevent memory leaks.
  for (auto& [name, vp] : window->views) {
    (void)name;
    delete vp;
  }
  window->views.clear();

  // Webview handle is owned by App — do NOT destroy here.
  window->webview = nullptr;

  delete window;
}

void showWindow(Window *window) {
  if (window == nullptr || window->webview == nullptr) {
    return;
  }

  // Guard against null config — use hardcoded defaults as fallback.
  int w = 1280;
  int h = 640;
  if (window->configs != nullptr) {
    w = window->configs->window_width;
    h = window->configs->window_height;
  }

  // Resizability: fixed or normal
  auto hint = (window->configs && !window->configs->resizable)
                  ? WEBVIEW_HINT_FIXED
                  : WEBVIEW_HINT_NONE;
  webview_set_size(window->webview, w, h, hint);

  // Minimum size constraints (0 = no constraint)
  if (window->configs && (window->configs->window_min_width > 0 ||
                          window->configs->window_min_height > 0)) {
    int mw = window->configs->window_min_width;
    int mh = window->configs->window_min_height;
    webview_set_size(window->webview, mw > 0 ? mw : 1, mh > 0 ? mh : 1,
                     WEBVIEW_HINT_MIN);
  }

  // Maximum size constraints (0 = no constraint)
  if (window->configs && (window->configs->window_max_width > 0 ||
                          window->configs->window_max_height > 0)) {
    int mw = window->configs->window_max_width;
    int mh = window->configs->window_max_height;
    webview_set_size(window->webview, mw, mh,
                     WEBVIEW_HINT_MAX);
  }

  const std::string& view_name = window->current_view;
  if (view_name.empty()) {
    webview_set_html(window->webview,
        "<!DOCTYPE html><html lang=\"en\"><body><h1>default View</h1></body></html>");
    return;
  }

  const auto it = window->views.find(view_name);
  if (it == window->views.end() || it->second == nullptr) {
    webview_set_html(window->webview,
        "<!DOCTYPE html><html lang=\"en\"><body><h1>View not found</h1></body></html>");
    return;
  }

  webview_set_html(window->webview, it->second->html.c_str());
}

void showView(Window *window, std::string name) {
  if (window == nullptr) {
    return;
  }

  // Defensive: only switch if the view exists; otherwise stay on current.
  if (window->views.count(name)) {
    window->current_view = std::move(name);
    showWindow(window);
  }
}

void addView(Window *window, std::string name, View *view) {
  if (window != nullptr && view != nullptr) {
    window->views[name] = view;
  }
}

std::expected<View, Error> createView(std::string pathOrCode, ViewKind kind,
                                      std::optional<ViewConfig> configs) {
  View view{.kind = kind};

  switch (kind) {
  case VIEW_KIND_FILE: {
    try {
      std::stringstream text_content;
      std::ifstream file(pathOrCode);
      if (!file.is_open()) {
        return std::unexpected(
            Error{.code = ErrorCode::MissingFile,
                  .message = "could not open file: " + pathOrCode});
      }
      std::string line;
      while (std::getline(file, line)) {
        text_content << line << '\n';
      }
      view.html = text_content.str();
    } catch (const std::exception &e) {
      return std::unexpected(
          Error{.code = ErrorCode::MissingFile,
                .message = std::string("file read error: ") + e.what()});
    }
    break;
  }
  case VIEW_KIND_HTML:
    view.html = pathOrCode;
    break;
  case VIEW_KIND_URL:
    return std::unexpected(Error{.code = ErrorCode::NotImplementedYet,
                                 .message = "URL views not yet implemented"});
  }

  // The Coconut frontend runtime is injected globally via webview_init()
  // in the transport layer — no longer per-view injection needed.

  return view;
}

/// Apply native window style based on Config (frameless, etc.).
/// For frameless windows on macOS, this hides the title bar while keeping
/// the traffic-light buttons, and makes the content fill the entire window.
void applyWindowStyle(Window *window) {
  if (window == nullptr || window->webview == nullptr ||
      window->configs == nullptr) {
    return;
  }

  auto* cfg = window->configs;

  if (cfg->frameless) {
    using id = struct objc_object*;
    using SEL = struct objc_selector*;

    id win = (id)webview_get_window(window->webview);
    if (!win) {
      debug::warn("applyWindowStyle: no native window handle");
      return;
    }

    // NSWindowStyleMask bit definitions:
    //   NSWindowStyleMaskTitled              = 1 << 0
    //   NSWindowStyleMaskClosable            = 1 << 1
    //   NSWindowStyleMaskMiniaturizable      = 1 << 2
    //   NSWindowStyleMaskResizable           = 1 << 3
    //   NSWindowStyleMaskFullSizeContentView  = 1 << 15

    // Read current style mask
    NSUInteger mask = ((NSUInteger(*)(id, SEL))objc_msgSend)(
        win, sel_registerName("styleMask"));

    // Add full-size content view so the webview fills behind the title bar
    mask |= (1UL << 15); // NSWindowStyleMaskFullSizeContentView

    // Keep titled so we retain the traffic-light buttons
    // but the title bar will be made transparent below.
    ((void(*)(id, SEL, NSUInteger))objc_msgSend)(
        win, sel_registerName("setStyleMask:"), mask);

    // Make the title bar transparent
    ((void(*)(id, SEL, BOOL))objc_msgSend)(
        win, sel_registerName("setTitlebarAppearsTransparent:"), (BOOL)YES);

    // Allow dragging the window from the webview content.
    // When enabled, clicking and dragging anywhere in the webview
    // moves the window. The frontend can toggle this via
    // ctx.window:setMovableByBackground(bool).
    // We default to YES for frameless windows and let the frontend
    // disable it on interactive elements (inputs, scroll areas, etc.).
    ((void(*)(id, SEL, BOOL))objc_msgSend)(
        win, sel_registerName("setMovableByWindowBackground:"), (BOOL)YES);

    debug::info("applyWindowStyle: applied frameless style");
  }
}

} // namespace coconut::window
