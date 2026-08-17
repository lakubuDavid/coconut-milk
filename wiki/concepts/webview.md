# webview

## What it is
A minimal cross-platform WebView wrapper (C++) that provides a native browser
engine on each OS: **WKWebView** (macOS), **WebView2** (Windows), **WebKitGTK**
(Linux). The project uses a vendored fork at `thirdparty/webview/`.

## Why we use it
- Zero bundled Chromium — uses the OS's native engine (~10 MB binary vs ~100 MB Electron)
- Single-window, single-page architecture matches the framework's design
- Built-in `webview_init` / `webview_bind` / `webview_eval` / `webview_return`
  primitives are exactly the bridge surface we need

## Key concepts
- **`webview_init(w, js)`** — injects JS before any page loads (used for the
  Coconut runtime shim)
- **`webview_bind(w, name, cb)`** — exposes a C++ function to JS as a Promise
- **`webview_eval(w, js)`** — one-way JS execution (fire-and-forget)
- **`webview_return(w, id, status, json)`** — resolves a bound Promise

## How we use it here
- `WebviewTransport` wraps these four primitives behind the `transport::Transport`
  interface
- `webview_init` injects `coconut.js` (the frontend runtime) at document start
- `webview_bind("__coconut_rpc", …)` is the single inbound channel for all
  JS→C++ messages (calls, events, list-views query)
- The `coconut://` URL scheme is registered via a pre-WKWebView configuration
  hook (patched into the webview fork — see [patches.md](patches.md))

## Gotchas
- **WKWebView navigation delegate must be installed AFTER the first page
  loads**, otherwise it intercepts the initial navigation and produces a white
  screen (fixed in `window.cpp:installNavDelegate`)
- **`webview_terminate` is synchronous** — calling it inside a `webview_bind`
  callback with no `webview_return` avoids `EXC_BAD_ACCESS` but is a narrow seam
  (documented in [ADR-0001](../decisions/ADR-0001-quit-via-synchronous-webview-terminate.md))
- Size hint constants (`WEBVIEW_HINT_NONE`, `WEBVIEW_HINT_MIN`,
  `WEBVIEW_HINT_MAX`) are platform-specific enum values
