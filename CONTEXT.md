# Context — Coconut Milk Domain Glossary

Terms the `improve-codebase-architecture` skill must use when analysing this codebase.
Do not use "component", "service", "API", or "boundary".

## Core Modules

**Runtime**
The Lua-authored application layer. Accessed via `coconut.config(ctx)` and `coconut.views()`.
The primary authoring surface for app behaviour.

**Bridge**
The RPC channel between the WebView (JS) and the Runtime (Lua). Carries `call`, `return`,
`error`, `event`, and `ready` envelopes defined in `src/rpc_envelope.h`.

**Transport**
Pluggable send/receive layer behind the bridge. Current: `WebviewTransport`.

**Platform layer**
Native code for each OS. Handles window creation, dialogs, clipboard, filesystem, scheme
handler, keybind monitoring, and notifications.

**Command**
A named Lua function registered via `ctx:bind(name, fn)`. One name maps to one handler.
Commands are the primary way the frontend calls into Lua.

**View**
A named HTML document registered via `coconut.views()`. Each view has a `name`, a `kind`
(url/html/file), and optional lifecycle callbacks (`on_load`, `on_mount`, `on_unmount`).

**Window**
The native platform window that hosts the WebView. Configured via `ctx:setWindowSize()`,
`ctx:setTitle()`, etc. Owned by the platform layer.

**Event**
A fire-and-forget message in either direction. Lua → frontend via `ctx:emit()`. Frontend
→ Lua via `coconut.emit()`. Separate namespace from commands.

**Keybind**
A keyboard shortcut. JS keybinds are registered via `coconut.keybind()`. Platform
keybinds (modifiers held) are consumed by an NSEvent monitor (macOS) before reaching the
WebView, preventing system beep. Platform → Lua → JS dispatch chain.

## Key Seams

| Seam | Between | Role |
|---|---|---|
| `Transport` | Bridge and native platform | Send/receive messages |
| `CoconutWindowHandle` | Lua runtime and platform window | Window control (close, fullscreen) |
| `scheme_handler.mm` | WebView and filesystem | Resolve `coconut://` URLs to files |
| `keyboard.mm` | macOS event loop and platform keybind registry | Consume registered combos |
| `lua_runtime.cpp` | Lua VM and bridge | Dispatch commands, emit events |

## Key Files

| File | Role |
|---|---|
| `src/lua_runtime.cpp` | Lua VM, command registration, event emission |
| `src/bridge.cpp` | RPC envelope, transport, dispatch |
| `src/platform/darwin/keyboard.mm` | NSEvent monitor, platform keybind consumption |
| `src/platform/darwin/scheme_handler.mm` | `coconut://` URL → file bytes |
| `src/platform/darwin/window_handle.mm` | Native window control |
| `src/embeds/coconut.ts` | JS embed (compiled to `coconut.js` → `coconut_embed.h`) |
| `examples/code-editor/` | Reference application |

## Known Architectural Tensions

- **Quit path**: `webview_terminate` is called synchronously inside the Lua `__coconutWindowCtl`
  handler. No `webview_return` is queued. This avoids `EXC_BAD_ACCESS` but is a narrow seam.
- **Platform keybinds**: JS registers combos via bridge → platform monitors consume them before
  WebView sees them. The two-step registration means the platform registry can be stale.
- **coconut:// scheme handler**: Must be registered before WKWebView creation. Hook lives in
  third-party `cocoa_wkwebview_engine.hh` via a static `on_configure_config` callback.
