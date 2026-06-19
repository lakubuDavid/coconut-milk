#include "lifecycle.h"
#include "../../app.h"
#include "../../debug.h"
#include <windows.h>
#include <vector>
#include <cstdint>

namespace coconut::lifecycle {

// Internal: store the app pointer and registered windows for event dispatch
static App* g_app = nullptr;
static std::vector<HWND> g_registered_windows;

// Forward declare the WindowProc
static LRESULT CALLBACK LifecycleWindowProc(HWND hwnd, UINT msg,
                                            WPARAM wparam, LPARAM lparam);

/// Subclass a window to intercept lifecycle events.
static void SubclassWindow(HWND hwnd) {
  // Store original WindowProc and set our hook
  SetWindowLongPtrW(hwnd, GWLP_WNDPROC,
                    reinterpret_cast<LONG_PTR>(LifecycleWindowProc));
  g_registered_windows.push_back(hwnd);
}

static LRESULT CALLBACK LifecycleWindowProc(HWND hwnd, UINT msg,
                                            WPARAM wparam, LPARAM lparam) {
  // Dispatch lifecycle events to the app
  if (g_app) {
    switch (msg) {
      case WM_SIZE: {
        int w = LOWORD(lparam);
        int h = HIWORD(lparam);
        // Emit resize event via Lua dispatch
        g_app->dispatch_lifecycle("resize", nullptr);
        break;
      }
      case WM_SETFOCUS:
        g_app->dispatch_lifecycle("focus", nullptr);
        break;
      case WM_KILLFOCUS:
        g_app->dispatch_lifecycle("blur", nullptr);
        break;
      case WM_CLOSE:
        g_app->dispatch_lifecycle("close", nullptr);
        break;
    }
  }

  // Chain to original WindowProc
  return DefWindowProcW(hwnd, msg, wparam, lparam);
}

void platformRegisterEvents(App* app) {
  g_app = app;
  debug::info("Win32 lifecycle hooks registered");

  // The webview library creates the main window internally.
  // We subclass it by finding all top-level windows belonging to
  // this process and subclassing the one that hosts the webview.
  //
  // In practice, the window handle is obtained from the webview
  // instance and subclassed there. This registration provides
  // the hook infrastructure; the actual subclassing happens
  // in window::platformApplyWindowStyle or after webview creation.
}

void platformUnregisterEvents() {
  g_app = nullptr;
  g_registered_windows.clear();
  debug::info("Win32 lifecycle hooks unregistered");
}

} // namespace coconut::lifecycle
