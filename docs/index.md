# Coconut Milk Documentation

A cross-platform desktop app framework powered by **Lua** for backend logic and **HTML/CSS/JS** for the UI. Build native desktop applications with a webview, not a browser runtime.

---

## 🚀 Getting Started

- **[Introduction](./getting-started.md#introduction)** — What is Coconut Milk and why use it?
- **[Installation](./getting-started.md#installation)** — Prerequisites, build from source, install binary
- **[create-coconut-app CLI](./getting-started.md#creating-your-first-project)** — Scaffold a new project
- **[Templates](./getting-started.md#templates)** — Bare, bare-ts, and Vite (React/Vue/Solid)
- **[Your First App](./getting-started.md#tutorial-your-first-app)** — Step-by-step tutorial
- **[Vite Integration](./getting-started.md#vite-integration)** — Hot reload, build pipeline, limitations
- **[Limitations](./getting-started.md#limitations)** — What Coconut Milk is *not* (yet)

## 📖 Concepts

- **[Architecture](./concepts.md#architecture)** — How the layers fit together (with diagrams)
- **[View System](./concepts.md#view-system)** — Named views, lifecycle, routing
- **[coconut:// Scheme](./concepts.md#the-coconut-scheme)** — Asset resolution, priority, how it works
- **[Event Model](./concepts.md#event-model)** — Events, pub/sub, emit vs emit_sync
- **[Bridge Protocol](./concepts.md#bridge-protocol)** — RPC envelopes, readiness handshake
- **[Config System](./concepts.md#config-system)** — `coconut.config.lua`, JSON schema, runtime config
- **[Platform Support](./concepts.md#platform-support)** — macOS, Windows, Linux status

## 🌙 Lua Backend Guide

- **[Commands](./lua-guide.md#commands)** — `ctx:bind()`, command files, `@command` annotations
- **[Config Callback](./lua-guide.md#config-callback)** — `coconut.config(ctx)`, window settings
- **[Views](./lua-guide.md#views)** — `coconut.views()`, view factories, lifecycle callbacks
- **[Events](./lua-guide.md#events)** — `ctx:emit()`, `coconut.events()`, lifecycle hooks
- **[Lua HTML DSL](./lua-guide.md#lua-html-dsl)** — Pure Lua template engine with `html.div{}`
- **[Patterns & Best Practices](./lua-guide.md#patterns--best-practices)** — Organization, error handling, state
- **[Limitations](./lua-guide.md#limitations)** — Single window, no threading model, table-only payloads

## 🌉 Bridge (Advanced)

- **[RPC Protocol](./bridge.md#rpc-protocol)** — Message types, envelope shape, JSON format
- **[Message Flow](./bridge.md#message-flow)** — `coconut.call()` path, `ctx:emit()` path
- **[Transport Layer](./bridge.md#transport-layer)** — Webview binding, script injection
- **[Readiness Handshake](./bridge.md#readiness-handshake)** — How `coconut.ready()` works
- **[Serialization](./bridge.md#serialization)** — JSON encoding, UTF-8 handling, sanitization
- **[Helpers](./bridge.md#frontend-helpers)** — `coconut.call()`, `coconut.emit()`, `coconut.on()`, `coconut.views()`, `coconut.ping()`
- **[Error Handling](./bridge.md#error-handling)** — Error codes, error envelopes, pcall safety
- **[TypeScript Definitions](./bridge.md#typescript-definitions)** — `coconut.d.ts`, generated `commands.d.ts`

## 📚 API Reference

- **[Lua API](./api-reference.md#lua-api)** — All functions, signatures, examples
  - `coconut.config(ctx)`, `coconut.views()`, `coconut.commands(ctx)`
  - Context methods (`setWindowSize`, `bind`, `emit`, `show`, etc.)
  - View factories (`View.load`, `View.html`, `View.url`)
  - View lifecycle (`on_load`, `on_mount`, `on_unmount`)
- **[JavaScript API](./api-reference.md#javascript-api)** — All functions, signatures, examples
  - `coconut.ready()`, `coconut.call()`, `coconut.emit()`, `coconut.on()`
  - `coconut.window.minimize()`, `.toggleFullscreen()`, `.close()`
  - `coconut.fs.readText()`
  - `coconut.views()`, `coconut.ping()`

## 🛠 CLI Reference

- **[coconut](./cli.md#coconut-binary)** — Run apps, generate, flags (`--help`, `--version`, `--debug`, `--root`)
- **[coconut generate](./cli.md#coconut-generate)** — Command generation, `--out-dir`
- **[create-coconut-app](./cli.md#create-coconut-app)** — Scaffolding CLI, templates, flags

## 💡 Examples

- **[Calculator (Vue)](./examples.md#calculator-vue)** — Multi-page Vue app with settings
- **[OCR Scanner](./examples.md#ocr-app)** — Image processing with Tesseract.js, Alpine.js, UnoCSS
- **[Code Editor](./examples.md#code-editor)** — CodeMirror 6, file tree, native dialogs
- **[Lua HTML App](./examples.md#lua-html-app)** — Pure Lua HTML DSL, no build step (see caveats — community pattern, not core)
- **[Edge Cases](./examples.md#edge-cases)** — Frameless windows, transparent backgrounds, view switching
- **[Tips & Tricks](./examples.md#tips--tricks)** — Debug mode, logging, FOUC prevention

## 🔧 Troubleshooting

- **[Common Errors](./troubleshooting.md#common-errors)** — `CommandNotFound`, `NotReady`, `MissingFile`
- **[Dialog Crashes](./troubleshooting.md#dialog-crashes)** — Open/save dialog issues, crash safety
- **[CORS / ESM Issues](./troubleshooting.md#cors--esm-module-issues)** — `coconut://` scripts, IIFE bundles
- **[Debugging](./troubleshooting.md#debugging)** — `--debug` flag, transport logs, WebKit inspector
- **[Platform Issues](./troubleshooting.md#platform-issues)** — macOS frameless, Windows/Linux stubs

---

## Project Links

- [GitHub Repository](https://github.com/lakubuDavid/coconut-milk)
- [Roadmap](./roadmap.md)
- [Specification](./specs.md)
- [Test Suite](./test-suite.md)
