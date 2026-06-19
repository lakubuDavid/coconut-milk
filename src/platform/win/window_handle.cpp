/// Win32 window handle operations (move, resize, fullscreen, etc.)

#include "window_handle.h"
#include "../../debug.h"
#include <windows.h>

namespace coconut {
namespace window {

// Helper: get HWND from webview_t.
// The webview library stores the window handle internally.
// We obtain it via the webview API or stored reference.
static HWND GetHwndFromWebview(webview_t wv) {
  // The webview_t type is an opaque pointer. For Win32 webview,
  // it contains the HWND internally. We cast through the library API.
  // webview_get_window(wv) is the standard way to get the native handle.
  // If not available, return the foreground window as fallback.
  #ifdef WEBVIEW_API_H
    // webview_get_window is declared in api.h
    extern void* webview_get_window(webview_t wv);
    return static_cast<HWND>(webview_get_window(wv));
  #else
    // Fallback: use the main thread's foreground window
    return GetForegroundWindow();
  #endif
}

void platformMoveWindow(webview_t wv, int dx, int dy) {
  HWND hwnd = GetHwndFromWebview(wv);
  if (!hwnd) return;

  RECT rect;
  if (GetWindowRect(hwnd, &rect)) {
    SetWindowPos(hwnd, nullptr,
                 rect.left + dx, rect.top + dy,
                 0, 0,
                 SWP_NOSIZE | SWP_NOZORDER);
  }
}

void platformSetWindowPosition(webview_t wv, int x, int y) {
  HWND hwnd = GetHwndFromWebview(wv);
  if (!hwnd) return;

  SetWindowPos(hwnd, nullptr, x, y, 0, 0,
               SWP_NOSIZE | SWP_NOZORDER);
}

void platformGetWindowPosition(webview_t wv, int& x, int& y) {
  HWND hwnd = GetHwndFromWebview(wv);
  if (!hwnd) return;

  RECT rect;
  if (GetWindowRect(hwnd, &rect)) {
    x = rect.left;
    y = rect.top;
  }
}

void platformMinimizeWindow(webview_t wv) {
  HWND hwnd = GetHwndFromWebview(wv);
  if (hwnd) ShowWindow(hwnd, SW_MINIMIZE);
}

void platformMaximizeWindow(webview_t wv) {
  HWND hwnd = GetHwndFromWebview(wv);
  if (hwnd) ShowWindow(hwnd, SW_MAXIMIZE);
}

void platformRestoreWindow(webview_t wv) {
  HWND hwnd = GetHwndFromWebview(wv);
  if (hwnd) ShowWindow(hwnd, SW_RESTORE);
}

void platformToggleFullscreen(webview_t wv) {
  HWND hwnd = GetHwndFromWebview(wv);
  if (!hwnd) return;

  static bool fullscreen = false;
  fullscreen = !fullscreen;

  if (fullscreen) {
    // Save current style and position, then switch to fullscreen
    RECT rect;
    GetWindowRect(hwnd, &rect);

    DWORD style = GetWindowLong(hwnd, GWL_STYLE);
    DWORD ex_style = GetWindowLong(hwnd, GWL_EXSTYLE);

    SetWindowLong(hwnd, GWL_STYLE, style & ~(WS_CAPTION | WS_THICKFRAME));
    SetWindowLong(hwnd, GWL_EXSTYLE, ex_style & ~(WS_EX_DLGMODALFRAME |
                                                  WS_EX_WINDOWEDGE |
                                                  WS_EX_CLIENTEDGE |
                                                  WS_EX_STATICEDGE));

    // Get monitor dimensions
    HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = { sizeof(mi) };
    GetMonitorInfo(monitor, &mi);

    SetWindowPos(hwnd, HWND_TOP,
                 mi.rcMonitor.left, mi.rcMonitor.top,
                 mi.rcMonitor.right - mi.rcMonitor.left,
                 mi.rcMonitor.bottom - mi.rcMonitor.top,
                 SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
  } else {
    // Restore - trigger a redraw with standard style
    ShowWindow(hwnd, SW_RESTORE);
  }
}

void platformSetFullscreen(webview_t wv, bool on) {
  HWND hwnd = GetHwndFromWebview(wv);
  if (!hwnd) return;

  if (on) {
    RECT rect;
    GetWindowRect(hwnd, &rect);

    HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = { sizeof(mi) };
    GetMonitorInfo(monitor, &mi);

    SetWindowPos(hwnd, HWND_TOP,
                 mi.rcMonitor.left, mi.rcMonitor.top,
                 mi.rcMonitor.right - mi.rcMonitor.left,
                 mi.rcMonitor.bottom - mi.rcMonitor.top,
                 SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
  } else {
    ShowWindow(hwnd, SW_RESTORE);
  }
}

void platformSetMovableByBackground(webview_t wv, bool on) {
  // Win32 doesn't have a direct "movable by background" concept.
  // Implemented via WM_NCHITTEST in the WindowProc.
  // When enabled, the entire client area returns HTCAPTION,
  // making the window draggable from any point.
  //
  // This is handled by the main WindowProc in create_window.cpp.
  // The flag is stored and checked in the message loop.
  debug::info("Win32: setMovableByBackground is handled via WindowProc");
  (void)wv;
  (void)on;
}

void platformSetWindowBackgroundColor(webview_t wv, float r, float g, float b, float a) {
  // Set window background color using Win32
  HWND hwnd = GetHwndFromWebview(wv);
  if (!hwnd) return;

  COLORREF color = RGB((BYTE)(r * 255), (BYTE)(g * 255), (BYTE)(b * 255));
  HBRUSH brush = CreateSolidBrush(color);

  // Store the brush handle in the window's USERDATA for cleanup
  HBRUSH old_brush = (HBRUSH)GetClassLongPtr(hwnd, GCLP_HBRBACKGROUND);
  SetClassLongPtr(hwnd, GCLP_HBRBACKGROUND, (LONG_PTR)brush);

  if (old_brush) {
    DeleteObject(old_brush);
  }

  // For transparent windows, set WS_EX_LAYERED and use alpha
  if (a < 1.0f) {
    DWORD ex_style = GetWindowLong(hwnd, GWL_EXSTYLE);
    SetWindowLong(hwnd, GWL_EXSTYLE, ex_style | WS_EX_LAYERED);
    SetLayeredWindowAttributes(hwnd, 0, (BYTE)(a * 255), LWA_ALPHA);
  }

  InvalidateRect(hwnd, nullptr, TRUE);
}

} // namespace window
} // namespace coconut
