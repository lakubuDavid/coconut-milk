/// Win32 window creation implementation.
///
/// Creates both frameless and standard windows using RegisterClassW
/// and CreateWindowExW.

#include "create_window.h"
#include "../../debug.h"

#if defined(_WIN32)
#include <windows.h>

// Window class name
static const wchar_t* kWindowClassName = L"CoconutMilkWindow";

// Forward declare WindowProc
static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg,
                                   WPARAM wparam, LPARAM lparam);

/// Register the window class once.
static bool RegisterWindowClass() {
  static bool registered = false;
  if (registered) return true;

  WNDCLASSW wc = {};
  wc.lpfnWndProc = WindowProc;
  wc.hInstance = GetModuleHandleW(nullptr);
  wc.lpszClassName = kWindowClassName;
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

  registered = RegisterClassW(&wc) != 0;
  if (!registered) {
    debug::error("Win32: Failed to register window class");
  }
  return registered;
}

static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg,
                                   WPARAM wparam, LPARAM lparam) {
  switch (msg) {
    case WM_DESTROY:
      PostQuitMessage(0);
      return 0;
    case WM_CLOSE:
      DestroyWindow(hwnd);
      return 0;
  }
  return DefWindowProcW(hwnd, msg, wparam, lparam);
}

static HWND CreateWin32Window(const wchar_t* title, int x, int y,
                              int w, int h, DWORD style, DWORD ex_style) {
  if (!RegisterWindowClass()) return nullptr;

  RECT rect = {0, 0, w, h};
  AdjustWindowRectEx(&rect, style, FALSE, ex_style);

  HWND hwnd = CreateWindowExW(
      ex_style,
      kWindowClassName,
      title,
      style,
      x, y,
      rect.right - rect.left,
      rect.bottom - rect.top,
      nullptr, nullptr,
      GetModuleHandleW(nullptr),
      nullptr);

  if (!hwnd) {
    debug::error("Win32: Failed to create window");
  }

  return hwnd;
}

void* coconut_create_frameless_window(int x, int y, int w, int h) {
  debug::info("Win32: Creating frameless window");

  DWORD style = WS_POPUP | WS_VISIBLE;
  DWORD ex_style = WS_EX_APPWINDOW;

  HWND hwnd = CreateWin32Window(L"Coconut Milk", x, y, w, h, style, ex_style);
  return hwnd;
}

void* coconut_create_standard_window(int x, int y, int w, int h) {
  debug::info("Win32: Creating standard window");

  DWORD style = WS_OVERLAPPEDWINDOW | WS_VISIBLE;
  DWORD ex_style = WS_EX_APPWINDOW;

  HWND hwnd = CreateWin32Window(L"Coconut Milk", x, y, w, h, style, ex_style);
  return hwnd;
}

const char* coconut_bundle_resource_path() {
  static std::string path;
  if (!path.empty()) return path.c_str();

  wchar_t exe_path[MAX_PATH];
  GetModuleFileNameW(nullptr, exe_path, MAX_PATH);

  // Get the directory containing the executable
  wchar_t* last_backslash = wcsrchr(exe_path, L'\\');
  if (last_backslash) {
    *last_backslash = L'\0';
  }

  // Convert to UTF-8
  int len = WideCharToMultiByte(CP_UTF8, 0, exe_path, -1,
                                nullptr, 0, nullptr, nullptr);
  path.resize(len - 1);
  WideCharToMultiByte(CP_UTF8, 0, exe_path, -1,
                      &path[0], len, nullptr, nullptr);

  return path.c_str();
}

#endif // _WIN32
