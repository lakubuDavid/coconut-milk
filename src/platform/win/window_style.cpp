/// Win32 window style application.
///
/// Reads Config to determine window attributes (frameless, transparent,
/// size, position) and applies them to the native window.

#include "window.h"
#include "../../config.h"
#include "../../debug.h"
#include <windows.h>

namespace coconut {
namespace window {

// Forward declare key combo mapping used in WindowProc
static std::string VKeyToCombo(DWORD vk, bool ctrl, bool alt, bool shift, bool win);

void platformApplyWindowStyle(webview_t wv, Config* cfg) {
  if (!cfg) return;

  debug::info("Win32: Applying window style");

  // Get the native window handle from the webview
  void* native_window = nullptr;
  // webview_get_window is the standard way, but may not be available.
  // We use a fallback approach.
  #ifdef WEBVIEW_API_H
    extern void* webview_get_window(webview_t wv);
    native_window = webview_get_window(wv);
  #else
    native_window = GetForegroundWindow();
  #endif

  if (!native_window) {
    debug::warn("Win32: Cannot get window handle for style application");
    return;
  }

  HWND hwnd = static_cast<HWND>(native_window);

  // Apply frameless style
  if (cfg->window.frameless) {
    debug::info("Win32: Setting frameless window");
    DWORD style = GetWindowLong(hwnd, GWL_STYLE);
    style &= ~(WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX |
               WS_MAXIMIZEBOX | WS_SYSMENU);
    SetWindowLong(hwnd, GWL_STYLE, style);
  }

  // Apply transparency
  if (cfg->window.transparent) {
    debug::info("Win32: Setting transparent window");
    DWORD ex_style = GetWindowLong(hwnd, GWL_EXSTYLE);
    SetWindowLong(hwnd, GWL_EXSTYLE, ex_style | WS_EX_LAYERED);

    // Use alpha value from config if available, default to 254 (near-opaque
    // for click-through vs actual transparency)
    BYTE alpha = 254;
    if (cfg->window.background_color.a < 1.0f) {
      alpha = static_cast<BYTE>(cfg->window.background_color.a * 255);
    }
    SetLayeredWindowAttributes(hwnd, 0, alpha, LWA_ALPHA);
  }

  // Apply window size if set
  if (cfg->window.width > 0 && cfg->window.height > 0) {
    SetWindowPos(hwnd, nullptr, 0, 0,
                 cfg->window.width, cfg->window.height,
                 SWP_NOMOVE | SWP_NOZORDER);
  }

  // Force a redraw
  SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
               SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
               SWP_FRAMECHANGED);
  InvalidateRect(hwnd, nullptr, TRUE);
}

void platformInstallNavDelegate(webview_t wv) {
  debug::info("Win32: Navigation delegate installation");
  // WebView2 navigation interception is done via
  // CoreWebView2.NavigationStarting event.
  // This requires the ICoreWebView2 interface obtained from
  // the webview library's internal WebView2 controller.
  //
  // The webview library (webview.cc) exposes a way to get the
  // ICoreWebView2 via webview_get_native_handle() or similar.
  //
  // For initial implementation, external URL interception is
  // handled entirely in the webview library itself.
  (void)wv;
}

void platformSetWindowBackgroundColor(webview_t wv, float r, float g, float b, float a) {
  HWND hwnd = nullptr;
  #ifdef WEBVIEW_API_H
    extern void* webview_get_window(webview_t wv);
    hwnd = static_cast<HWND>(webview_get_window(wv));
  #else
    hwnd = GetForegroundWindow();
  #endif
  if (!hwnd) return;

  COLORREF color = RGB((BYTE)(r * 255), (BYTE)(g * 255), (BYTE)(b * 255));
  HBRUSH brush = CreateSolidBrush(color);

  HBRUSH old_brush = (HBRUSH)SetClassLongPtr(hwnd, GCLP_HBRBACKGROUND,
                                               (LONG_PTR)brush);
  if (old_brush) DeleteObject(old_brush);

  if (a < 1.0f) {
    DWORD ex_style = GetWindowLong(hwnd, GWL_EXSTYLE);
    SetWindowLong(hwnd, GWL_EXSTYLE, ex_style | WS_EX_LAYERED);
    SetLayeredWindowAttributes(hwnd, 0, (BYTE)(a * 255), LWA_ALPHA);
  }

  InvalidateRect(hwnd, nullptr, TRUE);
}

} // namespace window
} // namespace coconut
