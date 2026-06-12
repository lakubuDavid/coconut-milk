# Coconut Milk 🥥

A **Lua-first, cross-platform desktop UI framework** with a native webview bridge.
Think Electron/Tauri, but minimal — single-window, Lua scripting, native webview.

```lua
-- main.lua
function coconut.config(ctx)
  ctx:setWindowSize({ w = 1280, h = 640 })
     :setInitialView("home")
  return ctx
end

function coconut.views()
  return {
    home = View.load("views/home.html"),
  }
end
```

---

## Features

- **Lua application layer** — sol2 bindings, full `ctx` API for window control, events, commands
- **macOS native** — frameless/transparent windows, WKWebView, `coconut://` custom URL scheme
- **Bridge protocol** — `coconut.call()`, `coconut.emit()`, `coconut.on()` between JS ↔ Lua
- **Command generation** — annotate Lua functions with `---@command`, get typed `.g.js` wrappers auto-generated
- **Single-window first** — no chrome, no multi-window complexity in v0
- **CLI** — `--help`, `--version`, `--debug`, `--root PATH`
- **Scaffolding** — `create-coconut-app` with `bare`, `bare-ts`, `vite` (Vue/React/Solid) templates
- **Install** — `just install` symlinks the binary + scaffold script

---

## Quick start

### Prerequisites

- C++20 toolchain (Clang 16+)
- [xmake](https://xmake.io) — build system
- [LuaJIT](https://luajit.org) (installed via xmake)
- [sol2](https://github.com/ThePhD/sol2) (installed via xmake)
- [Bun](https://bun.sh) — for bridge JS bundling
- Python 3 — for embed header generation

### Build & run

```bash
xmake build coconut
xmake run coconut
```

Or with the `calculator-vue` example:

```bash
just build-vue    # builds the Vue app first
just run-vue-prod # runs with pre-built production assets
```

### Install

```bash
just install
```

Symlinks `coconut` and `create-coconut-app` to `$HOME/tools/` (configurable).

### Scaffold a new app

```bash
create-coconut-app my-app -y
# or with a template:
create-coconut-app my-app --template bare-ts
create-coconut-app my-app --template vite --framework vue
```

---

## Architecture

```
┌─────────────────────────────────────────────────┐
│                  Lua App (main.lua)              │
│  coconut.config(ctx)  │  coconut.views()        │
│  coconut.commands(ctx)│  coconut.events(...)    │
└──────────┬───────────────────────────┬───────────┘
           │ ctx:bind, emit, show     │ View descriptors
           ▼                           ▼
┌──────────────────┐    ┌────────────────────────┐
│  Command Registry │    │  View System            │
│  (one handler/    │    │  file:// | inline HTML  │
│   command name)   │    │  | URL navigation       │
└──────┬───────────┘    └──────────┬─────────────┘
       │                            │
       └──────────┬─────────────────┘
                  ▼
┌─────────────────────────────────────────────────┐
│               Bridge (RPC Protocol)              │
│  call │ return │ error │ event │ ready          │
├─────────────────────────────────────────────────┤
│         Platform Transport (WKWebView)          │
│  __coconut_call / __coconut_emit / coconut.on   │
└─────────────────────────────────────────────────┘
                  │
                  ▼
┌─────────────────────────────────────────────────┐
│              Frontend (HTML/JS/TS/Vue)           │
│  coconut.call() │ coconut.emit() │ coconut.on() │
└─────────────────────────────────────────────────┘
```

### Modules

| Module | Role |
|---|---|
| `coconut::app` | Window + webview lifecycle |
| `coconut::lua` | Lua runtime, sol2 bindings, command dispatch |
| `coconut::bridge` | RPC transport, JS ↔ C++ message routing |
| `coconut::commands` | Named command registry |
| `coconut::window` | View system, navigation, window style |
| `coconut::config` | Config loading (Lua/JSON), CLI merge |
| `coconut::fs` | File I/O |
| `coconut::dialog` | Native dialogs (open/save/message) |
| `coconut::debug` | Structured logging |
| `coconut::error` | Error codes and result types |
| `coconut::lifecycle` | Window event observers (resize, focus, blur) |
| `coconut::platform` | Platform-specific (scheme handler, window style) |

### Directories

| Path | Purpose |
|---|---|
| `main.lua` | App entry point — `coconut.views()`, `coconut.config()` |
| `coconut.config.lua` | Configuration file |
| `views/` | HTML view assets |
| `commands/` | Lua command modules with `---@command` annotations |
| `assets/` | Static framework assets |
| `generated/` | Build output: `.g.lua`, `.d.ts`, `.g.js` |

### Build pipeline

Annotated Lua command files → **coconut generate** → typed wrappers:

```
commands/notes.lua
  → commands/notes.g.lua   (Lua registration glue)
  → commands/notes.d.ts    (TypeScript declarations)
  → commands/notes.g.js    (JS wrappers with JSDoc)
```

---

## Reference docs

- **[Specification](docs/specs.md)** — full API and bridge spec
- **[Roadmap](docs/roadmap.md)** — implementation plan and phases
- **[Test suite](docs/test-suite.md)** — test plan and test scope

### Support matrix

| Feature | macOS | Windows | Linux |
|---|---|---|---|
| Window creation | ✅ | ✅ | ✅ |
| WebView render | ✅ (WKWebView) | ✅ (WebView2) | ✅ (WebKitGTK) |
| `coconut://` scheme | ✅ | 🔲 stub | 🔲 stub |
| Frameless window | ✅ | 🔲 | 🔲 |
| Transparent BG | ✅ | 🔲 | 🔲 |
| Lua runtime | ✅ | ✅ | ✅ |
| Command generation | ✅ | ✅ | ✅ |
| Dialog (open/save) | ✅ | ✅ | ✅ |
| WKNavigationDelegate | ✅ | N/A | N/A |

✅ = working, 🔲 = planned

---

## Project status

**v0.1.0** — Active development. macOS is the primary target.
Windows/Linux implementations are functional for core features
but missing platform-specific polish.

See [the roadmap](docs/roadmap.md) for planned work.
