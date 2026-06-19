# ADR-0002: Windows Platform Support

**Status:** Draft

**Date:** 2026-06-19

**Authors:** Coconut Milk team

## Context

Coconut Milk is currently macOS-only. The project has Windows platform stubs in
`src/platform/win/` that are mostly empty implementations. We need to implement
full Windows support to make Coconut Milk a truly cross-platform desktop app
framework.

The goal is to:
1. Enable cross-compilation from macOS using MinGW + Wine for development
2. Implement all platform adapter modules for Windows
3. Use WebView2 as the Windows webview backend
4. Support testing under Wine where feasible, with real Windows for full validation

## Windows Platform Modules — Audit

### Module inventory

The darwin platform has 21 files (16 logical modules). The win platform has 11
files (7 modules). Missing modules are highlighted below.

| Module | Darwin | Win | Status |
|---|---|---|---|
| clipboard | `clipboard.h/.mm` | `clipboard.h/.cpp` | Stub — both windows open/read/write unimplemented |
| create_window | `create_window.h/.mm` | **MISSING** | No Win32 window creation module |
| dialog | `dialog.h/.mm` | `dialog.h/.cpp` | Stub — MessageBox, IFileDialog unimplemented |
| keyboard | `keyboard.h/.mm` | **MISSING** | No keyboard hook module (WindowProc message loop) |
| lifecycle | `lifecycle.cpp/.h` | `lifecycle.h/.cpp` | Stub — no-op implementations |
| notify | `notify.h/.mm` | `notify.h/.cpp` | Stub — Shell_NotifyIcon unimplemented |
| open_url | `open_url.h/.mm` | `open_url.h/.cpp` | Stub — ShellExecuteW unimplemented |
| permissions | `permissions.mm` | **MISSING** | No permissions module (macOS-specific APIs) |
| scheme_handler | Multi-file pattern | **MISSING** | No WebView2 resource request handler |
| window | `window.h/.cpp` + `window_style.mm` | `window.h` | Header-only stubs (inline no-ops) |
| window_handle | `window_handle.h/.mm` | **MISSING** | No window handle implementation (move, resize, fullscreen etc.) |

### Missing modules summary

- `create_window.h/.cpp` — Win32 window creation (RegisterClass, CreateWindowEx)
- `keyboard.h/.cpp` — Keyboard event hook (SetWindowsHookEx / WindowProc WM_KEYDOWN)
- `permissions.rc` — Not needed on Windows (no permission prompts for notifications)
- `scheme_handler_win.cpp` — WebView2 CoreWebView2.WebResourceRequested event
- `window_handle.h/.cpp` — Window manipulation (SetWindowPos, ShowWindow, etc.)
- `window_style.cpp` — Window style flags (WS_OVERLAPPEDWINDOW, WS_EX_LAYERED, etc.)

### macOS reference: what each darwin module does

#### clipboard
- `platformReadText()` → `[NSPasteboard generalPasteboard] stringForType:NSPasteboardTypeString`
- `platformWriteText(text)` → `[NSPasteboard generalPasteboard] clearContents; writeObjects`
- **Win32 equivalent:** `OpenClipboard` / `GetClipboardData(CF_UNICODETEXT)` / `SetClipboardData(CF_UNICODETEXT)` / `CloseClipboard`

#### create_window
- `createFramelessWindow(x, y, w, h)` → `NSWindow` with `NSBorderlessWindowMask`
- `createStandardWindow(x, y, w, h)` → `NSWindow` with standard titlebar
- `detectBundleResourcePath()` → checks `NSBundle.mainBundle.bundlePath`
- **Win32 equivalent:** `CreateWindowEx` with `WS_OVERLAPPEDWINDOW` or `WS_POPUP` for frameless; `GetModuleFileName` for bundle detection

#### dialog
- `platformMessageBox(title, msg, kind)` → `NSAlert` with appropriate style
- `platformOpenFile(...)` → `NSOpenPanel`
- `platformSaveFile(...)` → `NSSavePanel`
- **Win32 equivalent:** `MessageBox` / `IFileOpenDialog` / `IFileSaveDialog` (Common Item Dialog API)

#### keyboard
- `registerKeyboardMonitor(app, cb, userdata)` → `NSEvent addGlobalMonitorForEventsMatchingMask:NSKeyDownMask`
- `unregisterKeyboardMonitor()` → `NSEvent removeMonitor`
- **Win32 equivalent:** `SetWindowsHookEx(WH_KEYBOARD_LL, ...)` for global hook or `WM_KEYDOWN` in WindowProc for app-local

#### lifecycle
- `platformRegisterEvents(app)` → NSWindow delegate notifications (NSWindowDidResizeNotification, NSWindowDidBecomeKeyNotification, etc.)
- `platformUnregisterEvents()` → remove observers
- **Win32 equivalent:** Window subclassing / `WM_SIZE`, `WM_SETFOCUS`, `WM_KILLFOCUS` in WindowProc

#### notify
- `platformNotify(title, body)` → `UNUserNotificationCenter` request
- **Win32 equivalent:** `Shell_NotifyIcon` with `NIF_INFO` flag for balloon/toast notifications; Windows 10+ toast via COM `INotificationActivationCallback`

#### open_url
- `platformOpenUrl(url)` → `NSWorkspace.sharedWorkspace openURL:`
- **Win32 equivalent:** `ShellExecuteW(NULL, L"open", url, NULL, NULL, SW_SHOWNORMAL)`

#### permissions
- macOS-specific (privacy prompts for notifications, contacts, camera, etc.)
- **Win32:** Not needed. Windows does not have the same runtime permission model.
- Skip for initial implementation.

#### scheme_handler
- `installSchemeHandlerHook(root_dir)` → sets WKURLSchemeHandler configuration before webview create
- `finalizeSchemeHandler(wv)` → no-op on macOS (done in hook)
- **Win32 equivalent:** `CoreWebView2.WebResourceRequested` event after webview creation

#### window
- `platformApplyWindowStyle(wv, cfg)` → applies NSWindow style masks (frameless, transparent)
- `platformInstallNavDelegate(wv)` → WKNavigationDelegate for URL interception
- `platformSetWindowBackgroundColor(wv, r, g, b, a)` → NSWindow background color
- **Win32 equivalent:** Modified `CreateWindowEx` flags; `WebView2.NavigationStarting` event; `SetBackgroundColor` via Win32 message

#### window_handle
- `platformMoveWindow(wv, dx, dy)` → `[NSWindow setFrameOrigin:]`
- `platformSetWindowPosition(wv, x, y)` → `[NSWindow setFrameOrigin:]`
- `platformGetWindowPosition(wv, &x, &y)` → `[NSWindow frame].origin`
- `platformMinimizeWindow(wv)` → `[NSWindow miniaturize:]`
- `platformMaximizeWindow(wv)` → `[NSWindow zoom:]`
- `platformToggleFullscreen(wv)` → `[NSWindow toggleFullScreen:]`
- `platformSetFullscreen(wv, on)` → `[NSWindow setStyleMask:]`
- `platformSetMovableByBackground(wv, on)` → `[NSWindow setMovableByBackground:]`
- `platformSetWindowBackgroundColor(wv, r, g, b, a)` → `[NSWindow setBackgroundColor:]`
- **Win32 equivalent:** `SetWindowPos`, `GetWindowRect`, `ShowWindow(SW_MINIMIZE)`, `ShowWindow(SW_MAXIMIZE)`; fullscreen via `SetWindowLong` + `SetWindowPos`; `HTTRANSPARENT` + `WM_NCHITTEST` for movable-by-background

## Decision

### 1. Toolchain: MinGW cross-compiler on macOS + Wine for testing

We will use the MinGW-w64 cross-compiler toolchain installed via Homebrew:

```bash
brew install mingw-w64
```

This provides `x86_64-w64-mingw32-gcc` (GCC) and `x86_64-w64-mingw32-g++` for
cross-compiling to Windows PE format from macOS.

Xmake configuration:
```bash
xmake f -p windows -s x86_64 --toolchain=mingw
```

Wine 9.14 is already installed at `/opt/local/bin/wine` and can run the
compiled Windows binaries for testing.

### 2. WebView2 as the Windows webview backend

The `webview` third-party library already supports Windows via WebView2. When
compiled for Windows, it initializes the Edge WebView2 runtime. The library
uses `CoInitializeEx(COINIT_APARTMENTTHREADED)` and requires the WebView2
runtime to be installed on the target system.

WebView2 supports:
- `webview_create(debug, window)` — creates WebView2 instance
- `webview_navigate(w, url)` — navigate to URL
- `webview_set_html(w, html)` — load raw HTML
- `webview_eval(w, js)` — execute JavaScript
- `webview_init(w, js)` — inject JS at page start
- `webview_set_size(w, w, h)` — resize webview
- `webview_terminate(w)` — stop the message loop

### 3. Wine compatibility limitations

- **WebView2 under Wine:** WebView2 does NOT work under Wine. The Edge WebView2
  runtime requires the real Edge/Chromium renderer. Wine does not implement the
  WinRT COM interfaces that WebView2 depends on.
- **What CAN be tested under Wine:**
  - Clipboard (basic Win32 API works in Wine)
  - Dialog stubs (may work with Wine's open/save dialog shims)
  - Lifecycle hooks (message loop basics)
  - Notifications (partial — Wine has tray notification support)
  - OpenURL (ShellExecuteW typically works in Wine)
  - Window creation (basic Win32 windowing works in Wine)
  - Console/logic modules (fs, json, store, etc.)
- **What REQUIRES real Windows:**
  - WebView2 rendering
  - Full keyboard hook (WH_KEYBOARD_LL global hook)
  - Native notifications (Windows 10+ toast API)
  - Window transparency (WS_EX_LAYERED with UpdateLayeredWindow)
  - Frameless window (window shadow, resize handles)
- **Best approach:** Cross-compile with MinGW, test non-UI modules under Wine,
  then deploy to a real Windows machine (VM, CI runner, or bare metal) for
  full integration testing.

### 4. Implementation order

Implementation should follow the module dependency graph:

```
Phase 1 — Foundation (no Win32 dependencies)
├── open_url       ← ShellExecuteW, simple, no dependencies
├── clipboard      ← OpenClipboard/GetClipboardData/SetClipboardData
├── notify         ← Shell_NotifyIcon (basic balloon)
└── lifecycle      ← WindowProc hooks (WM_SIZE, focus events)

Phase 2 — Window system
├── create_window  ← RegisterClass + CreateWindowEx
├── window_style   ← Window style flags, transparency, frameless
├── window         ← ApplyWindowStyle wrapper
└── window_handle  ← SetWindowPos, ShowWindow, GetWindowRect

Phase 3 — Interactivity
├── keyboard       ← WH_KEYBOARD_LL or WindowProc WM_KEYDOWN
├── dialog         ← MessageBox, IFileOpenDialog, IFileSaveDialog
└── scheme_handler ← CoreWebView2.WebResourceRequested

Phase 4 — Polish
├── permissions    ← Not needed (skip)
└── bundle         ← Windows installer (.msi) and directory structure
```

### 5. xmake.lua changes — cross-compilation findings

The current `xmake.lua` already has Windows platform branches:

```lua
elseif is_plat("windows") then
    add_files("src/platform/win/*.cpp")
```

#### macOS → Windows cross-compilation (attempted)

MinGW-w64 was installed via Homebrew (`brew install mingw-w64`) but the bottle
(`mingw-w64--14.0.0_1.sonoma.bottle.tar.gz`) was built for macOS Sonoma x86_64.
On macOS Sequoia ARM64, the compiler binaries had unresolved `@@HOMEBREW_PREFIX@@`
rpath placeholders and the linker produced "file too short" errors on valid COFF
object files. This appears to be a bottle incompatibility with the host system.

**Recommendation:** Develop on real Windows or in a Windows VM for compilation.
The MinGW-w64 cross-compiler works reliably on actual Windows. CI can use
GitHub Actions with `windows-latest` runners for automated builds.

For macOS-based testing, the macos-native `coconut` binary can already be tested
under Wine for basic non-UI module validation (clipboard, open_url, lifecycle,
etc.), but full Windows compilation requires:
- A Windows machine (physical, VM, or CI runner)
- Visual Studio Build Tools (MSVC) or MinGW-w64 on Windows
- WebView2 runtime installed

### 6. Detailed module stubs

Each module should provide a minimal 'works under Wine' implementation first,
then be enhanced for full Windows fidelity later.

#### clipboard.h / clipboard.cpp
```cpp
// Win32 clipboard
std::string platformReadText() {
    if (!OpenClipboard(nullptr)) return {};
    HANDLE h = GetClipboardData(CF_UNICODETEXT);
    if (!h) { CloseClipboard(); return {}; }
    auto* p = static_cast<wchar_t*>(GlobalLock(h));
    if (!p) { CloseClipboard(); return {}; }
    std::wstring ws(p);
    GlobalUnlock(h);
    CloseClipboard();
    return std::string(ws.begin(), ws.end());
}
```

#### open_url.h / open_url.cpp
```cpp
bool platformOpenUrl(const std::string& url) {
    std::wstring wurl(url.begin(), url.end());
    HINSTANCE r = ShellExecuteW(nullptr, L"open", wurl.c_str(),
                                nullptr, nullptr, SW_SHOWNORMAL);
    return reinterpret_cast<INT_PTR>(r) > 32;
}
```

### 7. Testing strategy

| Module | Wine testable? | Real Windows needed? |
|---|---|---|
| clipboard | ✅ Yes | No — basic Win32 works under Wine |
| open_url | ✅ Yes | No — ShellExecuteW works under Wine |
| notify | ⚠️ Partial (balloon) | ✅ Yes (toast API) |
| lifecycle | ✅ Yes (basic) | ⚠️ WindowProc events work under Wine |
| create_window | ✅ Yes | ⚠️ Frameless/transparent needs real Windows |
| window_handle | ✅ Yes (basic) | ⚠️ Fullscreen needs real Windows |
| keyboard | ❌ No (global hook) | ✅ Yes (real Windows required) |
| dialog | ✅ Yes | No — Common dialogs work under Wine |
| scheme_handler | ❌ No | ✅ Yes (WebView2 requires real Windows) |
| permissions | N/A | N/A — skip for Windows |

## Consequences

### Positive
- Cross-compilation from macOS reduces Windows dev cycle friction
- Wine enables testing of ~70% of platform modules without a Windows machine
- Existing xmake.lua structure already supports the Windows branch
- WebView2 is a mature, well-supported Windows webview backend
- Module-by-module implementation provides clear milestones

### Negative
- WebView2 does not work under Wine — full integration testing requires real Windows
- MinGW has some differences from MSVC (exception handling, COM, threading)
- Some Win32 APIs are complex (Common Item Dialog, notification toast API)
- Keyboard global hook requires careful handling (low-level hook runs in context of SetWindowsHookEx caller)

### Neutral
- Permissions system is macOS-specific; Windows does not have the same model
- Bundle format will differ (Windows `.msi` or directory layout vs macOS `.app`)
- WebView2 runtime must be installed or bundled on target Windows machines

## Alternatives Considered

- **MSVC toolchain instead of MinGW** — MSVC requires Windows; cannot cross-compile from macOS.
  Rejected because MinGW enables iterative development on the primary dev machine.

- **Zig cross-compilation** — Zig has a broken installation on this system (abort trap).
  Rejected in favor of MinGW.

- **Real Windows VM for all development** — Minimizes toolchain issues but increases friction
  for quick iterations. Rejected because cross-compile + Wine is faster for module-level work.

- **Edge WebView2 → CEF (Chromium Embedded Framework)** — CEF is significantly larger and
  more complex to build. Rejected in favor of WebView2 which is already supported by the
  webview library.

- **Windows 7 support** — WebView2 requires Windows 10+. Windows 7 would need a different
  webview backend (e.g., IE WebBrowser control). Rejected as not worth the effort for v1.
