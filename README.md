<div align="center">
  <br>
  <h1 align="center">🥥 Coconut Milk</h1>
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

## 📱 Quick Start

### Prerequisites

| Tool | Version | Install |
|---|---|---|
| C++20 toolchain | Clang 16+ / GCC 13+ | `xcode-select --install` or system package |
| [xmake](https://xmake.io) | ≥ 2.8 | `brew install xmake` |
| [Bun](https://bun.sh) | ≥ 1.0 | `brew install oven-sh/bun/bun` |
| Python 3 | ≥ 3.10 | `brew install python3` |

### Build & Run

```bash
# Build the core binary
xmake build coconut

# Run with the calculator-vue example
just build-vue    # builds the Vue app first
just run-vue-prod # runs with pre-built production assets
```

### Install globally

```bash
just install
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

## ✨ Features

<div class="features-grid">
  <div class="feature-card">
    <h3>🟢 Lua Application Layer</h3>
    <p>Sol2 bindings, full <code>ctx</code> API for window control, events, and commands. LuaJIT boots in milliseconds.</p>
  </div>
  <div class="feature-card">
    <h3>🖥️ Native macOS Windows</h3>
    <p>Frameless, transparent windows powered by WKWebView with custom <code>coconut://</code> URL scheme support.</p>
  </div>
  <div class="feature-card">
    <h3>🔗 Bridge Protocol</h3>
    <p>Seamless RPC between JS and Lua: <code>coconut.call()</code>, <code>coconut.emit()</code>, <code>coconut.on()</code>.</p>
  </div>
  <div class="feature-card">
    <h3>⚡ Command Generation</h3>
    <p>Annotate Lua functions with <code>---@command</code> and get typed <code>.g.js</code> wrappers auto-generated.</p>
  </div>
  <div class="feature-card">
    <h3>📦 Single Binary</h3>
    <p>No Chromium, no Node.js. The entire runtime is a ~2–5 MB binary. Your app is just Lua + HTML.</p>
  </div>
  <div class="feature-card">
    <h3>🏗️ Scaffolding CLI</h3>
    <p><code>create-coconut-app</code> with bare, bare-ts, and Vite (Vue/React/Solid) templates.</p>
  </div>
</div>

---

## 🏗️ Architecture

```
┌─────────────────────────────────────────────────┐
│              Frontend (HTML/CSS/JS)              │
│  coconut.call() │ coconut.emit() │ coconut.on() │
└──────────────────────┬──────────────────────────┘
                       │  RPC Bridge
                       ▼
┌─────────────────────────────────────────────────┐
│         Platform WebView (WKWebView/WebView2)    │
│  Render HTML  │  Inject JS API  │  coconut://   │
└──────────────────────┬──────────────────────────┘
                       │  Message Transport
                       ▼
┌─────────────────────────────────────────────────┐
│              Lua Runtime (LuaJIT)                │
│  Commands  │  Events  │  Views  │  Config       │
└──────────────────────┬──────────────────────────┘
                       │  Native Bindings
                       ▼
┌─────────────────────────────────────────────────┐
│           Native Platform APIs                  │
│  Filesystem  │  Dialogs  │  Window  │  Clipboard│
└─────────────────────────────────────────────────┘
```

### Data Flow

1. **User interacts with UI** → frontend JS calls `coconut.call("cmd", payload)`
2. **WebView serializes** the call as JSON and sends it to the Lua runtime
3. **Lua handler executes** the command and returns a value (or emits events)
4. **Runtime serializes** the result back to JSON
5. **WebView delivers** the response → frontend Promise resolves

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
| `coconut::lifecycle` | Window event observers |
| `coconut::platform` | Platform-specific adapters |

---

## 📂 Project Layout

```
├── main.lua              # App entry point
├── coconut.config.lua    # Configuration file
├── views/                # HTML view assets
├── commands/             # Lua command modules
│   └── *.lua             #   with ---@command annotations
├── assets/               # Static framework assets
├── generated/            # Build output:
│   ├── *.g.lua           #   Lua registration glue
│   ├── *.d.ts            #   TypeScript declarations
│   └── *.g.js            #   JS wrappers with JSDoc
├── wiki/                 # Documentation site
├── src/                  # C++ source code
└── tests/                # Test suite
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

## 📚 Documentation

| Section | Description |
|---|---|
| [📖 Getting Started](wiki/getting-started.md) | Installation, first app, templates |
| [🧠 Concepts](wiki/explanation/concepts.md) | Architecture, view system, bridge protocol |
| [📘 Lua Backend Guide](wiki/reference/lua-guide.md) | Commands, events, views, best practices |
| [🔧 Bridge Reference](wiki/reference/bridge.md) | RPC protocol, message flow, transport |
| [📋 API Reference](wiki/reference/api-reference.md) | All Lua and JS APIs with signatures |
| [💻 CLI Reference](wiki/reference/cli.md) | coconut, generate, create-coconut-app |
| [📐 Specs](wiki/reference/specs/specs.md) | Full specification documents |
| [🧪 Test Suite](wiki/reference/test-suite.md) | Test plan and coverage |
| [🗺️ Roadmap](wiki/explanation/roadmap.md) | Implementation plan and phases |

---

## 🖥️ Platform Support

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

## 🚀 Examples

| Example | Stack | Features |
|---|---|---|
| [Calculator Vue](wiki/examples/examples.md#calculator-vue) | Vue 3 + Vite | Multi-page, settings |
| [OCR Scanner](wiki/examples/examples.md#ocr-app) | Alpine.js + Tesseract.js | Image processing |
| [Code Editor](wiki/examples/examples.md#code-editor) | CodeMirror 6 | File tree, native dialogs |
| [Lua HTML App](wiki/examples/examples.md#lua-html-app) | Pure Lua DSL | No build step |

---

## 🤝 Contributing

Contributions are welcome! Please see our [Roadmap](wiki/explanation/roadmap.md) for planned work and the [Specification](wiki/reference/specs/specs.md) for architecture details.

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

---

## 📄 License

Distributed under the MIT License. See [`LICENSE`](LICENSE) for more information.

---

<div align="center">
  <p>
    <strong>🥥 Coconut Milk</strong> —
    <a href="https://github.com/lakubuDavid/coconut-milk">GitHub</a> •
    <a href="wiki/">Documentation</a> •
    <a href="wiki/explanation/roadmap.md">Roadmap</a>
  </p>
  <p>
    <sub>Built with ❤️ using C++20, LuaJIT, and WKWebView</sub>
  </p>
</div>
