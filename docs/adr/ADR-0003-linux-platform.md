---
layout: default
title: ADR-0003 — Linux Platform
parent: Architecture Decision Records
nav_order: 2
description: Linux platform implementation decisions.
---

# ADR-0003: Linux Platform Support

**Status:** Draft

**Date:** 2026-06-19

**Authors:** Coconut Milk team

## Context

Coconut Milk is currently macOS-only. The project has Linux platform stubs in
`src/platform/linux/` that are mostly empty implementations (returning false or
empty values). We need to implement full Linux support to make Coconut Milk a
truly cross-platform desktop app framework.

The goal is to:
1. Enable native builds on Linux (Ubuntu 22.04+/Debian 12+ as primary targets)
2. Implement all platform adapter modules for Linux using GTK3 + WebKitGTK
3. Use Lima VM for local development on macOS; GitHub Actions + xvfb for CI
4. Support headless testing for modules that don't require a physical display

### Why GTK3 over GTK4

GTK3 is preferred for the initial implementation because:
- **Wider compatibility:** GTK3 is available on Ubuntu 20.04 LTS+, Debian 11+, Fedora 34+
- **WebKitGTK 4.1:** The `webkit2gtk-4.1` package is the stable, well-tested API version
- **GTK4 + WebKitGTK 6.0** is newer and less battle-tested; some platforms don't ship it yet
- **webview library** already supports both; our GTK3 code can be upgraded to GTK4 later

## Linux Platform Modules — Audit

### Module inventory

The darwin platform has 21 files (16 logical modules). The linux platform has 11
files (7 modules, all stubs). Missing modules are highlighted below.

| Module | Darwin | Linux | Status |
|---|---|---|---|
| clipboard | `clipboard.h/.mm` | `clipboard.h/.cpp` | Stub — GTK3 clipboard unimplemented |
| create_window | `create_window.h/.mm` | **NOT NEEDED** | Window creation delegated to webview library (`webview_create(NULL)`) |
| dialog | `dialog.h/.mm` | `dialog.h/.cpp` | Stub — GtkMessageDialog / GtkFileChooserNative unimplemented |
| keyboard | `keyboard.h/.mm` | **MISSING** | No keyboard hook module (GtkEventControllerKey) |
| lifecycle | `lifecycle.cpp/.h` | `lifecycle.h/.cpp` | Stub — no GTK signal connections |
| notify | `notify.h/.mm` | `notify.h/.cpp` | Stub — libnotify unimplemented |
| open_url | `open_url.h/.mm` | `open_url.h/.cpp` | Stub — gtk_show_uri_on_window / xdg-open unimplemented |
| permissions | `permissions.mm` | **NOT NEEDED** | Linux doesn't have the same runtime permission model |
| scheme_handler | Multi-file pattern | `scheme_handler.cpp` (root) | Shared stub at src/platform/scheme_handler.cpp — needs Linux branch |
| window | `window.h/.cpp` + `window_style.mm` | `window.h` | Header-only inline stubs — needs real implementations |
| window_handle | `window_handle.h/.mm` | **MISSING** | No GtkWindow manipulation (move, resize, fullscreen etc.) |

### Missing modules summary

- `keyboard.h/.cpp` — GtkEventControllerKey for key event monitoring
- `window_handle.h/.cpp` — GtkWindow manipulation (gtk_window_move, gtk_window_resize, etc.)
- `window.cpp` — GtkWindow style functions (decorated, fullscreen, background color)

### macOS reference: what each darwin module does

#### clipboard
- `platformReadText()` → `[NSPasteboard generalPasteboard] stringForType:NSPasteboardTypeString`
- `platformWriteText(text)` → `[NSPasteboard generalPasteboard] clearContents; setString`
- **Linux equivalent:** `gtk_clipboard_get(GDK_SELECTION_CLIPBOARD)` + `gtk_clipboard_wait_for_text()` / `gtk_clipboard_set_text()`

#### create_window
- macOS-specific: `NSWindow` creation with `NSBorderlessWindowMask`
- **Linux:** NOT NEEDED. The webview library (`gtk_webkit_engine`) creates the GtkWindow
  automatically when `webview_create(debug, NULL)` is called. If an existing window
  is passed, the webview embeds into it. The macOS `create_window` is an
  artifact of needing to configure NSWindow before webview creation.

#### dialog
- `platformMessageBox(title, msg, kind)` → `NSAlert`
- `platformOpenFile(...)` → `NSOpenPanel`
- `platformSaveFile(...)` → `NSSavePanel`
- **Linux equivalent:** `gtk_message_dialog_new()` for message boxes; `GtkFileChooserNative`
  for open/save dialogs (supports both GTK3 and GTK4 via the GtkNativeDialog interface)

#### keyboard
- `registerKeyboardMonitor(app, cb, userdata)` → `NSEvent addLocalMonitorForEventsMatchingMask:NSKeyDownMask`
- `unregisterKeyboardMonitor()` → `NSEvent removeMonitor`
- **Linux equivalent:** `gtk_event_controller_key_new()` attached to the GtkWindow;
  `g_signal_connect` on `key-pressed` / `key-released` events

#### lifecycle
- `platformRegisterEvents(app)` → NSWindow delegate notifications (resize, focus, blur)
- `platformUnregisterEvents()` → remove observers
- **Linux equivalent:** `g_signal_connect` on GtkWindow for `configure-event` (resize),
  `focus-in-event` / `focus-out-event` (focus/blur)

#### notify
- `platformNotify(title, body)` → `NSUserNotification` / `UNUserNotificationCenter`
- **Linux equivalent:** `libnotify` — `notify_notification_new()` + `notify_notification_show()`

#### open_url
- `platformOpenUrl(url)` → `NSWorkspace.sharedWorkspace openURL:`
- **Linux equivalent:** `gtk_show_uri_on_window()` (GTK3/GTK4 native) or `xdg-open` via `fork()`/`exec()`

#### permissions
- macOS-specific (privacy prompts for notifications, contacts, camera, etc.)
- **Linux:** Not needed. Linux does not have the same runtime permission model
  (notifications, filesystem access are granted at install/user level).
  Skip for initial implementation.

#### scheme_handler
- `installSchemeHandlerHook(root_dir)` → sets WKURLSchemeHandler configuration before webview create
- `finalizeSchemeHandler(wv)` → no-op on macOS
- **Linux equivalent:** `webkit_web_context_register_uri_scheme()` after webview creation,
  handled in `finalizeSchemeHandler()`. The handler callback serves coconut:// resources
  from the filesystem root directory.

#### window
- `platformApplyWindowStyle(wv, cfg)` → applies NSWindow style masks (frameless, transparent)
- `platformInstallNavDelegate(wv)` → WKNavigationDelegate for URL interception
- `platformSetWindowBackgroundColor(wv, r, g, b, a)` → NSWindow background color
- **Linux equivalent:** `gtk_window_set_decorated()` for frameless; `webkit_web_view`
  navigation policy decision handler for URL interception; CSS/GTK background for color

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
- **Linux equivalent:** `gtk_window_move()`, `gtk_window_get_position()`,
  `gtk_window_resize()`, `gtk_window_iconify()` (minimize), `gtk_window_maximize()`,
  `gtk_window_fullscreen()` / `gtk_window_unfullscreen()`, CSS background

## Decision

### 1. Toolchain: Native GCC/Clang on Linux

Unlike Windows (which requires MinGW cross-compilation), Linux uses the native
platform toolchain. Development can happen:

- **Locally via Lima VM:** Install build dependencies, xmake, and compile natively
- **CI via GitHub Actions:** Ubuntu 22.04 or 24.04 runners with all dependencies
- **Natively:** If the developer is on Linux, build directly

No cross-compilation needed. The standard xmake workflow applies:
```bash
sudo apt install libgtk-3-dev libwebkit2gtk-4.1-dev libnotify-dev liblua5.1-dev
xmake f -p linux
xmake
```

### 2. WebKitGTK 4.1 as the webview backend

The `webview` third-party library already supports Linux via WebKitGTK. When
compiled for Linux, it uses the `gtk_webkit_engine` which handles window
creation, webview embedding, JavaScript evaluation, and bindings.

WebKitGTK 4.1 supports:
- `webview_create(debug, window)` — creates a GtkWindow + WebKitWebView
- `webview_navigate(w, url)` — navigate via `webkit_web_view_load_uri()`
- `webview_set_html(w, html)` — load raw HTML via `webkit_web_view_load_html()`
- `webview_eval(w, js)` — execute JavaScript via `webkit_web_view_evaluate_javascript()`
- `webview_init(w, js)` — inject JS via `WebKitUserScript`
- `webview_set_size(w, w, h)` — resize via `gtk_window_resize()`
- `webview_terminate(w)` — stop the GLib main loop
- `webview_get_window(w)` — returns `GtkWindow*`
- `webview_get_native_handle(w, BROWSER_CONTROLLER)` — returns `WebKitWebView*`

### 3. Lima VM for local development on macOS

For macOS developers, Lima is the recommended local VM solution.

**Setup:**
```bash
brew install lima
limactl create --name coconut-linux template://ubuntu-24.04
limactl start coconut-linux
```

**Dependencies inside VM:**
```bash
sudo apt update
sudo apt install -y build-essential git xmake curl \
  libgtk-3-dev libwebkit2gtk-4.1-dev libnotify-dev \
  libluajit-5.1-dev liblua5.1-dev pkg-config
```

**File sharing:**
Edit `~/.lima/coconut-linux/lima.yaml` and set:
```yaml
mounts:
  - location: "~/Code"
    writable: true
    mountType: virtiofs
```

**X11 forwarding for GUI testing:**
```bash
# On macOS host, install XQuartz
# SSH with X11 forwarding:
limactl shell coconut-linux
# Or directly:
ssh -X -p 60022 localhost
# Then run GUI apps from the SSH session
```

### 4. GUI testing limitations under Lima

| Module | Testable under Lima (X11) | Notes |
|---|---|---|
| clipboard | ✅ Yes | GTK clipboard works over X11 |
| dialog | ✅ Yes | GtkDialog renders via X11 forwarding |
| lifecycle | ✅ Yes | GtkWindow signals fire normally |
| open_url | ✅ Yes | xdg-open or gtk_show_uri_on_window |
| notify | ⚠️ Partial | libnotify may not display; fallback to log |
| keyboard | ✅ Yes | GtkEventControllerKey works |
| window | ✅ Yes | GtkWindow operations work |
| window_handle | ✅ Yes | gtk_window_move/resize work |
| scheme_handler | ⚠️ Partial | WebKitGTK loads but rendering is slow over X11 |
| webview rendering | ❌ Slow | WebKitGTK over X11 forwarding is sluggish |
| Full integration test | ❌ | Needs CI (GitHub Actions + xvfb) or real hardware |

### 5. CI strategy: GitHub Actions + xvfb

Full integration testing (including WebKitGTK rendering) will be done in CI.

**Workflow:**
```yaml
- name: Install dependencies
  run: |
    sudo apt update
    sudo apt install -y libgtk-3-dev libwebkit2gtk-4.1-dev \
      libnotify-dev xvfb

- name: Build
  run: xmake f -p linux && xmake

- name: Test
  run: xvfb-run xmake test
```

xvfb provides a virtual X11 framebuffer, allowing GTK3/WebKitGTK applications
to run headlessly. This is the standard approach for testing GTK apps in CI.

### 6. Implementation order

Implementation follows dependency order:

```
Phase 1 — Foundation (no window dependency)
├── open_url       ← simple, no window needed (uses xdg-open)
├── clipboard      ← GTK clipboard, no window needed
├── notify         ← libnotify, simple
└── lifecycle      ← GtkWindow signal connections

Phase 2 — Window system
├── window         ← GtkWindow style (decorated, background)
├── window_handle  ← gtk_window_move, resize, fullscreen
└── window.h       ← Update header stubs → real implementations

Phase 3 — Interactivity
├── keyboard       ← GtkEventControllerKey for key event monitoring
├── dialog         ← GtkMessageDialog, GtkFileChooserNative
└── scheme_handler ← webkit_web_context_register_uri_scheme
```

### 7. xmake.lua changes

The current `xmake.lua` already has Linux platform branches:
```lua
elseif is_plat("linux") then
    add_files("src/platform/linux/*.cpp")
```

Changes needed:
1. Add Linux-specific packages: `gtk3`, `webkit2gtk-4.1`, `libnotify`
2. Link against GTK3 and WebKitGTK libraries via `pkg-config`
3. Pass `WEBVIEW_GTK` and `WEBVIEW_PLATFORM_LINUX` defines
4. The `webview` third-party target needs GTK link flags

### 8. Out-of-scope for v1

The following are deferred:
- **GTK4 support:** The initial implementation targets GTK3 for maximum compatibility.
  GTK4 + WebKitGTK 6.0 can be added later.
- **Wayland-specific features:** GTK3's GDK backend handles X11/Wayland transparently.
  Wayland-specific optimizations (e.g., wl_shell for fullscreen) are deferred.
- **Snap/Flatpak packaging:** Desktop integration/distribution packaging is a separate effort.
- **DBus activation:** Single-instance enforcement via DBus is not yet implemented.
- **Global keyboard shortcuts:** `gtk_application` accelerators are not used;
  keyboard is handled via `GtkEventControllerKey` on the window.

## Consequences

### Positive
- Native Linux builds use the platform's standard toolchain (no cross-compilation)
- GTK3 is widely available and well-tested across distributions
- WebKitGTK 4.1 is the stable, documented WebKit API for GTK3
- The xmake build system already supports Linux platform branching
- Lima enables macOS developers to build and test Linux module code locally
- GitHub Actions + xvfb provides CI-level integration testing
- Most platform modules can be developed and tested incrementally

### Negative
- GTK3 does not integrate with Wayland-native features (e.g., fractional scaling, protocols)
- libnotify notifications may not display under X11 forwarding (requires real session bus)
- WebKitGTK rendering over X11 forwarding (Lima) is too slow for practical use
- Full integration testing requires either CI or a physical/virtual Linux machine
- GTK3 is increasingly legacy-focussed; future migration to GTK4 will be needed

### Neutral
- Permissions system is macOS-specific; Linux does not have the same runtime model
- The `create_window` module is not needed on Linux (webview library handles it)
- Bundle format differs (AppImage/Flatpak/snap vs macOS .app)
- WebKitGTK must be installed on target systems (often available by default on GNOME desktops)

## Alternatives Considered

- **GTK4 + WebKitGTK 6.0** — Newer API but less widely available across distributions.
  Ubuntu 22.04 LTS ships GTK3 and webkit2gtk-4.1 but not GTK4/webkitgtk-6.0.
  Rejected for v1 because GTK3 maximizes compatibility.

- **QT6 + QtWebEngine** — Cross-platform toolkit alternative. Would require replacing
  the GTK-based webview library with a Qt-based backend. Rejected because the project
  already has a working webview abstraction built on webview/GTK.

- **Docker for all development** — Faster than VMs but WebKitGTK cannot run in Docker
  (requires real kernel DRM/DRI devices, D-Bus session bus). Rejected because we need
  actual GTK/WebKitGTK execution.

- **Multipass instead of Lima** — Canonical's Ubuntu VM tool. Equivalent to Lima but
  Ubuntu-only and slower file I/O (9p vs virtiofs). Rejected in favor of Lima's
  better performance and distro flexibility.

- **Self-hosted Linux CI runner on macOS developer's network** — Could use a Raspberry Pi
  or NUC running Ubuntu as a build/test target. Deferred as unnecessary complexity for v1.

- **x11docker for CI testing** — x11docker runs GUI apps in Docker containers. While
  useful for quick tests, it cannot run WebKitGTK (needs kernel features). Rejected
  in favor of xvfb on native GitHub Actions runners.

- **Alpine Linux** — Smaller image size but lacks webkit2gtk-4.1 package.
  Rejected in favor of Ubuntu/Debian for maximum package availability.
