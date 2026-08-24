<div align="center">
  <br>
  <h1 align="center">Coconut Milk</h1>
  <p align="center">
    <strong>Lua-first, cross-platform desktop UI framework</strong>
    <br>
    Think Electron/Tauri, but minimal — single-window, Lua scripting, native webview.
  </p>
  <br>

  [![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
  [![Build Status](https://img.shields.io/badge/build-passing-brightgreen.svg)](#)
  [![Version](https://img.shields.io/badge/version-0.1.0-orange.svg)](#)
  [![macOS](https://img.shields.io/badge/platform-macos-lightgrey.svg)](#)
  [![Windows](https://img.shields.io/badge/platform-windows-blue.svg)](#)
  [![Linux](https://img.shields.io/badge/platform-linux-yellow.svg)](#)

  <br>

  <a href="#quick-start">Quick Start</a> •
  <a href="#features">Features</a> •
  <a href="wiki/">Documentation</a> •
  <a href="wiki/explanation/roadmap.md">Roadmap</a> •
  <a href="wiki/reference/specs/specs.md">Specification</a>

  <br>
  <br>

  <pre><code># macOS & Linux
git clone https://github.com/lakubuDavid/coconut-milk.git
cd coconut-milk
xmake build coconut && xmake run coconut</code></pre>

  <br>
</div>

---

## Quick Start

### Prerequisites

| Tool | Version | Install |
|---|---|---|
| C++20 toolchain | Clang 16+ / GCC 13+ | `xcode-select --install` or system package |
| [xmake](https://xmake.io) | ≥ 2.8 | `brew install xmake` |
| [Bun](https://bun.sh) | ≥ 1.0 | `brew install oven-sh/bun/bun` |
| Python 3 | ≥ 3.10 | `brew install python3` |

### Build & Run

All build/run/test flows go through mise (which also pins tool versions):

```bash
mise run build        # configure (debug) + build the coconut binary
mise run run          # build + run the app
mise run test         # build + run the 346-test suite
mise run build-asan   # AddressSanitizer build
```

### Install globally

```bash
mise run install
```

This symlinks `coconut` and `create-coconut-app` to `$HOME/tools/` (configurable).

### Scaffold a new app

```bash
create-coconut-app my-app -y
# or with a template:
create-coconut-app my-app --template bare-ts
create-coconut-app my-app --template vite --framework vue
```

---

## Features

<div class="features-grid">
  <div class="feature-card">
    <h3>🟢 Lua Application Layer</h3>
    <p>Sol2 bindings, full <code>ctx</code> API for window control, events, and commands. LuaJIT boots in milliseconds.</p>
  </div>
  <div class="feature-card">
    <h3>Native macOS Windows</h3>
    <p>Frameless, transparent windows powered by WKWebView with custom <code>coconut://</code> URL scheme support.</p>
  </div>
  <div class="feature-card">
    <h3>🔗 Bridge Protocol</h3>
    <p>Seamless RPC between JS and Lua: <code>coconut.call()</code>, <code>coconut.emit()</code>, <code>coconut.on()</code>.</p>
  </div>
  <div class="feature-card">
    <h3>Command Generation</h3>
    <p>Annotate Lua functions with <code>---@command</code> and get typed <code>.g.js</code> wrappers auto-generated.</p>
  </div>
  <div class="feature-card">
    <h3>📦 Single Binary</h3>
    <p>No Chromium, no Node.js. The entire runtime is a ~2–5 MB binary. Your app is just Lua + HTML.</p>
  </div>
  <div class="feature-card">
    <h3>Scaffolding CLI</h3>
    <p><code>create-coconut-app</code> with bare, bare-ts, and Vite (Vue/React/Solid) templates.</p>
  </div>
</div>

---

## Architecture

Rendered diagrams: [`coconut_relationships.svg`](docs/architecture/coconut_relationships.svg) ·
[`coconut_sequence_command_result.svg`](docs/architecture/coconut_sequence_command_result.svg)
(sources: `docs/architecture/*.d2`, regenerate with `mise x -- d2 <file>.d2 <file>.svg`)

```
┌──────────────────────────────────────────────────────┐
│                Frontend (HTML/CSS/JS)                 │
│  coconut.call() ─ __coconut_rpc(id,name,payload)      │
│  replies via __coconut_rpc_receive({id,type,payload}) │
└───────────────────────┬─────────────────────────────┘
                        │  WKScriptMessageHandler
                        ▼
┌──────────────────────────────────────────────────────┐
│  WebviewTransport ──► core::Bridge.onInbound          │
│     kEvent → emitToLua (coconut._dispatch)            │
│     kCall  → sync executor (mt/main registries)       │
│            → else Dispatcher → WorkerPool             │
└───────────────────────┬─────────────────────────────┘
                        │  uniformly async envelopes
                        ▼
┌──────────────────────────────────────────────────────┐
│   Main thread: dispatch pump (CFRunLoopSource)        │
│   flush() + task queue (dispatch::post from workers)  │
└───────────────────────┬─────────────────────────────┘
                        ▼
┌──────────────────────────────────────────────────────┐
│  Workers (Lua VM each) · platform/* native ops        │
└──────────────────────────────────────────────────────┘
```

### Data Flow (worker command with correlation)

1. **JS** calls `coconut.call("cmd", params)` — the shim generates a unique id and parks a promise
2. **Bridge::onInbound** — main-thread-only commands answer instantly; others queue to the Dispatcher carrying the id as `RpcId`
3. **Worker executes** the Lua handler in its own VM (sandboxed); results come back as `Resolve/Reject{RpcId}`
4. **Dispatcher flush** turns them into `kReturn {ok,data}` / `kError {ok,error}` envelopes
5. **`__coconut_rpc_receive(id)`** resolves the parked promise — one reply mechanism for everything

### Modules

| Module | Role |
|---|---|
| `coconut::app` | Window + webview lifecycle, owns the core trio |
| `coconut::lua` (main_runtime) | Lua runtime, sol2 bindings, builtin `bind_mt` commands |
| `core::Bridge` | Inbound routing (`onInbound`), sync executor, envelope replies |
| `core::Dispatcher` | Typed message queue → WorkerPool; flushes on main |
| `core::WorkerPool` | Background Lua workers (one VM each, per-worker registries) |
| `dispatch` | Main-thread pump: CFRunLoopSource, `post()`, task queue |
| `WebviewTransport` | `__coconut_rpc` binding + envelope send/eval |
| `coconut::window` (module) | Thread-aware window API — direct on main, marshalled from workers |
| `coconut::commands` | Named command registry (`handlers` + `mt_handlers`) |
| `coconut::window` / `view_events` | View system, navigation, lifecycle events |
| `coconut::config` | Config loading (Lua/JSON), CLI merge |
| `coconut::fs` · `dialog` · `debug` · `error` · `lifecycle` | Core services |
| `coconut::platform` | Per-OS adapters behind `platform/window_native.h`, `platform/runloop.h` |

---

## Project Layout (monorepo)

```
├── apps/
│   ├── coconut/               # GUI runtime binary
│   │   ├── src/               #   C++ source (core/, modules/, platform/)
│   │   ├── tests/             #   346-test suite + e2e
│   │   └── xmake.lua
│   └── coconut-cli/           # generator + scaffolder CLI (std-only, LLVM 22)
├── samples/                   # canonical sample app (commands/, views/, generated/)
├── examples/                  # calculator-vue, playground, code-editor, …
├── schemas/                   # config schema + TS declarations
├── scripts/                   # create-coconut-app, install helpers
├── docs/architecture/         # rendered .svg diagrams (.d2 sources)
├── wiki/                      # guides / concepts / reference / specs
└── mise.toml                  # monorepo root: monorepo_root=true, config_roots
```

Inside a project directory (`samples/`, your app):

```
├── main.lua              # App entry point
├── coconut.config.lua    # Configuration file
├── views/                # HTML view assets
├── commands/             # Lua command modules
│   └── *.lua             #   with ---@command annotations
├── generated/            # Build output:
│   ├── *.g.lua           #   worker-thread registration glue
│   ├── *.g_mt.lua        #   main-thread registration glue
│   ├── *.d.ts            #   TypeScript declarations
│   └── *.g.js            #   JS wrappers with JSDoc
└── assets/               # Static assets (coconut://assets/…)
```

### Build Pipeline

Annotated Lua commands → **`coconut generate`** → typed wrappers:

```
commands/notes.lua
  → commands/notes.g.lua   (Lua registration glue)
  → commands/notes.d.ts    (TypeScript declarations)
  → commands/notes.g.js    (JS wrappers with JSDoc)
```

---

## Documentation

| Section | Description |
|---|---|
| [📖 Getting Started](wiki/getting-started.md) | Installation, first app, templates |
| [🧠 Concepts](wiki/explanation/concepts.md) | Architecture, view system, bridge protocol |
| [📘 Lua Backend Guide](wiki/reference/lua-guide.md) | Commands, events, views, best practices |
| [Bridge Reference](wiki/reference/bridge.md) | RPC protocol, message flow, transport |
| [📋 API Reference](wiki/reference/api-reference.md) | All Lua and JS APIs with signatures |
| [💻 CLI Reference](wiki/reference/cli.md) | coconut, generate, create-coconut-app |
| [📐 Specs](wiki/reference/specs/specs.md) | Full specification documents |
| [🧪 Test Suite](wiki/reference/test-suite.md) | Test plan and coverage |
| [Roadmap](wiki/explanation/roadmap.md) | Implementation plan and phases |

---

## Platform Support

| Feature | macOS | Windows | Linux |
|---|---|---|---|
| Window creation | ✅ | ✅ | ✅ |
| WebView render | ✅ WKWebView | ✅ WebView2 | ✅ WebKitGTK |
| `coconut://` scheme | ✅ | 🔲 Stub | 🔲 Stub |
| Frameless window | ✅ | 🔲 | 🔲 |
| Transparent BG | ✅ | 🔲 | 🔲 |
| Lua runtime | ✅ | ✅ | ✅ |
| Command generation | ✅ | ✅ | ✅ |
| Native dialogs | ✅ | ✅ | ✅ |

✅ = working &nbsp;&nbsp; 🔲 = planned

---

## Examples

| Example | Stack | Features |
|---|---|---|
| [Calculator Vue](wiki/examples/examples.md#calculator-vue) | Vue 3 + Vite | Multi-page, settings |
| [OCR Scanner](wiki/examples/examples.md#ocr-app) | Alpine.js + Tesseract.js | Image processing |
| [Code Editor](wiki/examples/examples.md#code-editor) | CodeMirror 6 | File tree, native dialogs |
| [Lua HTML App](wiki/examples/examples.md#lua-html-app) | Pure Lua DSL | No build step |

---

## Contributing

Contributions are welcome! Please see our [Roadmap](wiki/explanation/roadmap.md) for planned work and the [Specification](wiki/reference/specs/specs.md) for architecture details.

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

---

## License

Distributed under the MIT License. See [`LICENSE`](LICENSE) for more information.

---

<div align="center">
  <p>
    <strong>Coconut Milk</strong> —
    <a href="https://github.com/lakubuDavid/coconut-milk">GitHub</a> •
    <a href="wiki/">Documentation</a> •
    <a href="wiki/explanation/roadmap.md">Roadmap</a>
  </p>
  <p>
    <sub>Built with using C++20, LuaJIT, and WKWebView</sub>
  </p>
</div>
