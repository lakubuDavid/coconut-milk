---
layout: home
title: Home
nav_order: 1
description: >-
  Coconut Milk documentation — build cross-platform desktop apps with Lua and web technologies.
permalink: /
---

# 🥥 Coconut Milk Documentation

A cross-platform desktop app framework powered by **Lua** for backend logic and **HTML/CSS/JS** for the UI. Build native desktop applications with a webview, not a browser runtime.

{: .fs-5 .fw-300 }
Think Electron/Tauri, but minimal — single-window, Lua scripting, native webview.

---

## Getting Started

{: .d-flex .flex-row .flex-wrap .gap-4 }

<div class="card" markdown="1">
{: .p-4 }

### 📖 [Introduction](getting-started#introduction)
What is Coconut Milk and why use it?

</div>

<div class="card" markdown="1">
{: .p-4 }

### 🔧 [Installation](getting-started#installation)
Prerequisites, build from source, install binary.

</div>

<div class="card" markdown="1">
{: .p-4 }

### 🚀 [create-coconut-app CLI](getting-started#creating-your-first-project)
Scaffold a new project in seconds.

</div>

<div class="card" markdown="1">
{: .p-4 }

### 📦 [Templates](getting-started#templates)
Bare, bare-ts, and Vite (React/Vue/Solid).

</div>

<div class="card" markdown="1">
{: .p-4 }

### 👋 [Your First App](getting-started#tutorial-your-first-app)
Step-by-step tutorial.

</div>

<div class="card" markdown="1">
{: .p-4 }

### ⚡ [Vite Integration](getting-started#vite-integration)
Hot reload, build pipeline, limitations.

</div>

---

## Core Concepts

{: .d-flex .flex-row .flex-wrap .gap-4 }

<div class="card" markdown="1">
{: .p-4 }

### 🏗️ [Architecture](explanation/concepts#architecture)
How the layers fit together (with diagrams).

</div>

<div class="card" markdown="1">
{: .p-4 }

### 🖼️ [View System](explanation/concepts#view-system)
Named views, lifecycle, routing.

</div>

<div class="card" markdown="1">
{: .p-4 }

### 🔗 [coconut:// Scheme](explanation/concepts#the-coconut-scheme)
Asset resolution, priority, how it works.

</div>

<div class="card" markdown="1">
{: .p-4 }

### 📡 [Event Model](explanation/concepts#event-model)
Events, pub/sub, emit vs emit_sync.

</div>

<div class="card" markdown="1">
{: .p-4 }

### 🌉 [Bridge Protocol](explanation/concepts#bridge-protocol)
RPC envelopes, readiness handshake.

</div>

<div class="card" markdown="1">
{: .p-4 }

### ⚙️ [Config System](explanation/concepts#config-system)
`coconut.config.lua`, JSON schema, runtime config.

</div>

---

## Reference

{: .d-flex .flex-row .flex-wrap .gap-4 }

<div class="card" markdown="1">
{: .p-4 }

### 📘 [Lua Backend Guide](reference/lua-guide)
Commands, events, views, best practices.

</div>

<div class="card" markdown="1">
{: .p-4 }

### 🔧 [Bridge (Advanced)](reference/bridge)
RPC protocol, message flow, transport layer.

</div>

<div class="card" markdown="1">
{: .p-4 }

### 📋 [API Reference](reference/api-reference)
All Lua and JavaScript APIs with signatures.

</div>

<div class="card" markdown="1">
{: .p-4 }

### 💻 [CLI Reference](reference/cli)
coconut, generate, create-coconut-app.

</div>

<div class="card" markdown="1">
{: .p-4 }

### 📐 [Specifications](reference/specs/specs)
Full specification documents.

</div>

<div class="card" markdown="1">
{: .p-4 }

### 🧪 [Test Suite](reference/test-suite)
Test plan and coverage.

</div>

---

## Guides & Troubleshooting

{: .d-flex .flex-row .flex-wrap .gap-4 }

<div class="card" markdown="1">
{: .p-4 }

### 🎯 [How-to Guides](guide/)
Practical guides for events, keybinds, debugging, bundling.

</div>

<div class="card" markdown="1">
{: .p-4 }

### 🐛 [Troubleshooting](explanation/troubleshooting)
Common errors, debugging tips, platform issues.

</div>

<div class="card" markdown="1">
{: .p-4 }

### 📊 [Event Dispatch Model](explanation/event-dispatch-model)
Deep dive into event routing and queues.

</div>

<div class="card" markdown="1">
{: .p-4 }

### 🗺️ [Roadmap](explanation/roadmap)
Implementation plan and phases.

</div>

---

## Examples

<div class="card" markdown="1">
{: .p-4 }

### 🧮 [Calculator Vue](examples/examples#calculator-vue)
Multi-page Vue app with settings persistence.

### 🔍 [OCR Scanner](examples/examples#ocr-app)
Image processing with Tesseract.js, Alpine.js, UnoCSS.

### ✏️ [Code Editor](examples/examples#code-editor)
CodeMirror 6, file tree, native dialogs.

### 🔬 [Lua HTML App](examples/examples#lua-html-app)
Pure Lua HTML DSL, no build step.

</div>

---

## Platform Support

| Feature | macOS | Windows | Linux |
|---|---|---|---|
| Window creation | ✅ | ✅ | ✅ |
| WebView render | ✅ WKWebView | ✅ WebView2 | ✅ WebKitGTK |
| `coconut://` scheme | ✅ | 🔲 stub | 🔲 stub |
| Frameless window | ✅ | 🔲 | 🔲 |
| Transparent BG | ✅ | 🔲 | 🔲 |
| Lua runtime | ✅ | ✅ | ✅ |
| Command generation | ✅ | ✅ | ✅ |
| Dialog (open/save) | ✅ | ✅ | ✅ |
| WKNavigationDelegate | ✅ | N/A | N/A |

<div class="label label-green">✅ = working</div> &nbsp;
<div class="label label-yellow">🔲 = planned</div>

---

## Project Links

- [GitHub Repository](https://github.com/lakubuDavid/coconut-milk)
- [Specification](reference/specs/specs)
- [Roadmap](explanation/roadmap)
- [Test Suite](reference/test-suite)
