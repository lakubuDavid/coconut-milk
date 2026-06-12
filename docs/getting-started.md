# Getting Started with Coconut Milk

## Introduction

Coconut Milk is a **cross-platform desktop app framework** that combines:

- **Lua** for backend logic (commands, filesystem, native dialogs)
- **HTML/CSS/JS** for the UI (any framework — vanilla, Vue, React, Solid, Alpine.js)
- **Native webview** for rendering (WKWebView on macOS, WebView2 on Windows, WebKitGTK on Linux)

Think of it as a lightweight alternative to Electron or Tauri, but with Lua instead of Rust/Node.js. The entire runtime is a single `coconut` binary (~2-5 MB depending on platform), and your app is just a folder of Lua scripts and HTML files.

### Why Lua?

- **Fast startup** — LuaJIT boots in milliseconds
- **Simple syntax** — Easy to learn, great for scripting
- **Hot-reload friendly** — No compilation step for Lua code
- **Low memory** — ~200KB runtime footprint
- **Extensible** — sol2 C++ bindings make native calls trivial

### Why not just use a browser?

- **Native window** — Frameless, transparent, custom titlebar
- **Native dialogs** — File open/save, message boxes
- **Filesystem access** — Read/write files, list directories
- **No browser chrome** — No URL bar, no dev tools by default
- **Single binary** — No Node.js, no Chromium, no 200 MB bundle

---

## Installation

### Prerequisites

| Tool | Why | Install |
|---|---|---|
| **C++20 toolchain** | Build the native runtime | Clang 16+ (macOS) or GCC 13+ (Linux) |
| **xmake** | Build system (CMake alternative) | `brew install xmake` (macOS) or [xmake.io](https://xmake.io) |
| **Bun** | JavaScript bundling for code-editor example | `brew install oven-sh/bun/bun` or [bun.sh](https://bun.sh) |
| **Python 3** | Embed header generation | `brew install python3` |
| **LuaJIT** (optional) | Run `create-coconut-app` locally | `brew install luajit` |

### Build from Source

```bash
# Clone the repository
git clone https://github.com/lakubuDavid/coconut-milk.git
cd coconut-milk

# Build the core binary (coconut, webview, tests)
xmake build coconut
```

This produces:

```
build/macosx/x86_64/debug/coconut   # The main binary
```

### Install to `$HOME/tools`

```bash
just install
```

This symlinks two tools into `~/tools/`:

```
~/tools/coconut             → coconut runtime binary
~/tools/create-coconut-app  → project scaffolding script
```

Add `~/tools` to your `PATH` for global access:

```bash
# Add to ~/.zshrc or ~/.bashrc
export PATH="$HOME/tools:$PATH"
```

### Verify Installation

```bash
coconut --version
# Output: 0.1.0

create-coconut-app --help
# Shows template options and flags
```

---

## Creating Your First Project

The `create-coconut-app` CLI scaffolds a new Coconut Milk project.

### Quick Start

```bash
create-coconut-app my-app
cd my-app
coconut
```

This creates a minimal app with an HTML view and a "Hello World" command.

### CLI Flags

```
create-coconut-app [project-name] [options]

Options:
  -t, --template <bare|bare-ts|vite>   Template type (default: bare)
  -f, --framework <vanilla|react|vue|solid>  Framework for vite template (default: vanilla)
  -p, --pm <npm|bun>                   Package manager for vite template (default: npm)
  -y, --yes                            Skip prompts, use defaults
  -h, --help                           Show this help
```

### Interactive Mode

If you run without flags, the CLI prompts you:

```bash
create-coconut-app
Project name: [my-coconut-app] my-ocr-tool
Template:
  1) bare (default)
  2) bare-ts
  3) vite
Choice [bare]: 1

Done! Created project in ./my-ocr-tool
```

### Template Selection

For automated setup, use flags:

```bash
# Minimal HTML + CSS + JS (no build step)
create-coconut-app my-app --template bare

# Same as bare, but with TypeScript support
create-coconut-app my-app --template bare-ts

# Vite + Vue.js (hot reload in dev)
create-coconut-app my-app --template vite --framework vue

# Vite + React + Bun
create-coconut-app my-app --template vite --framework react --pm bun

# Non-interactive, all defaults
create-coconut-app my-app -y
```

---

## Templates

### Bare Template

The simplest possible setup. No build step, no dependencies, no framework.

```bash
create-coconut-app my-app --template bare
```

**Project structure:**

```
my-app/
├── main.lua              # Entry point
├── coconut.config.lua    # Optional config file
├── coconut.d.ts          # TypeScript type definitions
├── tsconfig.json         # JS type-checking config (checkJs: true)
├── commands/
│   ├── example.lua       # Lua command with @command annotations
│   └── example.g.js      # Generated JS wrapper (auto-generated)
└── views/
    ├── index.html        # Main view
    ├── style.css         # Styles
    └── app.js            # Frontend logic
```

**Running:**

```bash
cd my-app
coconut
```

The binary reads `main.lua` from the current working directory, loads views, and opens the window.

### Bare TypeScript Template

Same as bare, but with proper TypeScript support:

```bash
create-coconut-app my-app --template bare-ts
```

**Key differences:**

- `src/app.ts` instead of `views/app.js`
- `tsconfig.json` with `outDir: "dist"`, `rootDir: "src"`
- Use `tsc` to compile TypeScript to JavaScript before running
- `coconut.d.ts` included for type hints on `window.coconut`

**Running with TypeScript:**

```bash
cd my-app
tsc                    # Compiles src/app.ts → dist/app.js
coconut                # Loads views/index.html which references dist/app.js
```

### Vite Template

Full Vite + framework setup with hot reload:

```bash
create-coconut-app my-app --template vite --framework vue
```

**Project structure:**

```
my-app/
├── main.lua              # Entry point (dev + prod views)
├── coconut.config.lua    # Config with dev/prod views
├── package.json          # Vite + framework dependencies
├── vite.config.js        # Pre-configured with base: './'
├── index.html            # Vite entry point
├── src/
│   ├── main.js           # Framework entry
│   └── App.vue           # Vue component (or React/Solid)
├── commands/
│   ├── example.lua       # Lua commands
│   └── example.g.js      # Generated wrappers
└── dist/                 # Production build output (created by `npm run build`)
```

**Dev mode (hot reload):**

```bash
# Terminal 1: Start Vite dev server
npm run dev
# Vite runs on http://localhost:5173

# Terminal 2: Run Coconut with dev flag
COCONUT_DEV=1 coconut
# Coconut loads View.url("http://localhost:5173") for dev
```

**Production mode:**

```bash
npm run build              # Produces dist/
coconut                    # Loads View.load("dist/index.html") for prod
```

### How Vite Dev Mode Works

The `main.lua` generated by the Vite template checks for an environment variable:

```lua
local dev = os.getenv("COCONUT_DEV") == "1"

function coconut.views()
  return {
    app = View.load("dist/index.html"),       -- Production: load built files
    dev = View.url("http://localhost:5173"),  -- Dev: load from Vite server
  }
end

function coconut.config(ctx)
  return ctx
    :setInitialView(dev and "dev" or "app")   -- Switch view based on env
    -- ...
end
```

When `COCONUT_DEV=1`, Coconut opens the Vite dev server URL in the webview. Vite's hot module replacement (HMR) works normally — changes to your Vue/React/Solid code instantly update in the webview.

### Vite Base URL

The generated `vite.config.js` includes `base: './'`:

```js
export default defineConfig({
  plugins: [vue()],
  base: './',  // Critical for file:// compatibility
})
```

This ensures all asset paths are relative, so the production build works when loaded from `file://` (not just `http://`).

---

## Tutorial: Your First App

Let's build a simple greeting app from scratch.

### Step 1: Scaffold the Project

```bash
create-coconut-app hello-app --template bare -y
cd hello-app
```

### Step 2: Edit the View

Open `views/index.html`:

```html
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Hello App</title>
  <link rel="stylesheet" href="coconut://views/style.css">
</head>
<body>
  <h1>Hello, <span id="name">World</span>!</h1>
  <input id="input" placeholder="Enter your name" />
  <button id="btn">Greet</button>
  <p id="response"></p>

  <script src="coconut://views/app.js"></script>
</body>
</html>
```

**Note:** We use `coconut://` URLs for assets. These resolve relative to the app root, not the view's directory. See [The coconut:// Scheme](./concepts.md#the-coconut-scheme) for details.

### Step 3: Write the Frontend Logic

Edit `views/app.js`:

```js
// Wait for the bridge to be ready
await coconut.ready()

// Get DOM elements
const input = document.getElementById('input')
const btn = document.getElementById('btn')
const response = document.getElementById('response')
const nameSpan = document.getElementById('name')

// Call Lua command when button clicked
btn.addEventListener('click', async () => {
  const name = input.value || 'World'
  try {
    // Call the 'greet' command in Lua
    const result = await coconut.call('greet', { name })
    response.textContent = result.greeting
    nameSpan.textContent = name
  } catch (err) {
    response.textContent = `Error: ${err.message}`
  }
})

// Listen for events from Lua
coconut.on('greeted', (payload) => {
  console.log(`Greeted: ${payload.name}`)
})
```

### Step 4: Write the Lua Command

Create `commands/greet.lua`:

```lua
---@command greet
---@param params { name?: string }
---@return { greeting: string }
local function greet(params, ctx)
  local name = params.name or "World"
  local greeting = "Hello, " .. name .. "!"

  -- Emit an event back to the frontend
  ctx:emit("greeted", { name = name })

  return { greeting = greeting }
end

return { greet = greet }
```

The `---@command greet` annotation tells the code generator to:
1. Register this command with the bridge
2. Generate TypeScript types in `generated/greet.d.ts`
3. Generate a JS wrapper in `generated/greet.g.js`

### Step 5: Configure the App

Edit `main.lua`:

```lua
function coconut.config(ctx)
  return ctx
    :setWindowSize({ w = 600, h = 400 })
    :setMinimumWindowSize({ w = 400, h = 300 })
    :setTitle("Hello App")
    :setResizable(true)
    :setInitialView("app")
end

function coconut.views()
  return {
    app = View.load("views/index.html"),
  }
end
```

### Step 6: Generate Type Wrappers

```bash
coconut generate
```

This scans `commands/*.lua`, extracts `@command` annotations, and generates:

```
generated/
├── greet.d.ts       # TypeScript types
├── greet.g.js       # JavaScript wrapper
└── greet.g.lua      # Lua registration glue
└── commands.d.ts    # Aggregated command name union
```

### Step 7: Run

```bash
coconut
```

A 600×400 window opens with your HTML view. Type a name, click "Greet", and the Lua command responds with a greeting.

### Step 8: Try Events

In the frontend, we added `coconut.on('greeted', ...)`. The Lua command calls `ctx:emit("greeted", { name })`, which triggers the frontend listener. This demonstrates the two-way communication:

- **Frontend → Lua**: `coconut.call()` returns a Promise
- **Lua → Frontend**: `ctx:emit()` fires event listeners

---

## Vite Integration

### Full Setup with Vue

If you're using Vue (or React/Solid) with Vite:

1. **Create the project:**

```bash
create-coconut-app my-vue-app --template vite --framework vue -y
cd my-vue-app
```

2. **Install dependencies:**

```bash
npm install    # or: bun install
```

3. **Start Vite dev server (Terminal 1):**

```bash
npm run dev
# Vite runs on http://localhost:5173
```

4. **Run Coconut (Terminal 2):**

```bash
COCONUT_DEV=1 coconut
```

Coconut opens `http://localhost:5173` in the webview. Changes to `.vue` files hot-reload instantly.

5. **Build for production:**

```bash
npm run build
# Produces dist/ folder
coconut
# Loads dist/index.html
```

### Using Generated Command Helpers

After running `coconut generate`, import the generated helpers in your Vite app:

```ts
// src/App.vue or src/main.ts
import { greet } from '../commands/example.g.js'

async function handleClick() {
  const result = await greet({ name: 'Ada' })
  console.log(result.greeting)  // "Hello, Ada!"
}
```

The generated helpers are thin wrappers around `coconut.call()`:

```js
// commands/example.g.js (generated)
export async function greet(payload) {
  return coconut.call("greet", payload)
}
```

### Vite Limitations

| Feature | Status | Notes |
|---|---|---|
| **HMR in dev** | ✅ Works | Vite injects WebSocket for hot reload |
| **Production build** | ✅ Works | `base: './'` ensures `file://` compatibility |
| **ESM modules** | ⚠️ Limited | `type="module"` from `coconut://` fails CORS. Bundle as IIFE instead |
| **Import aliases** | ⚠️ Limited | Vite `@/` aliases work in dev but may break in prod |
| **Server-side rendering** | ❌ Not supported | Webview is client-side only |

### ESM vs IIFE

The biggest gotcha with Vite + Coconut: **ESM modules don't work from `coconut://`**.

**Why:** The page loads from `file://` but scripts load from `coconut://`. These are different origins, and `type="module"` scripts require CORS. WKWebView silently discards cross-origin ESM modules.

**Solution:** Use IIFE bundles:

```bash
# Instead of vite build (ESM), bundle as IIFE:
bun build src/main.js --outfile assets/bundle.js --format iife
```

Or configure Vite to output IIFE (not default). For most projects, the development workflow works fine with Vite HMR, and production builds can use the standard Vite output with IIFE bundling.

---

## Limitations

### Current Limitations (v0.1)

| Feature | Status | Workaround |
|---|---|---|
| **Single window** | ✅ Only one window supported | Use view switching (`ctx:show()`) for multi-page |
| **System tray** | ❌ Not implemented | Planned for future |
| **Menu bar** | ❌ Not implemented | Use HTML/CSS for custom menus |
| **Notifications** | ❌ Not implemented | Use HTML/CSS for in-app toasts |
| **Clipboard** | ❌ Not implemented | Use JS `navigator.clipboard` API |
| **Threading** | ❌ Single-threaded Lua | Commands run synchronously |
| **Windows/Linux scheme handler** | 🔲 Stub | `coconut://` only works on macOS (WKWebView) |
| **Frameless window on Windows/Linux** | 🔲 Stub | Only macOS supports frameless/transparent |
| **Plugin system** | ❌ Not implemented | Use `commands/` folder for extensibility |
| **Auto-updates** | ❌ Not implemented | Plan for distribution phase |

### Payload Limitations

- **Table-only payloads**: All bridge payloads must be Lua tables / JS objects
- **No raw primitives**: `ctx:emit("event", "string")` won't work — use `ctx:emit("event", { message = "string" })`
- **JSON serialization**: All payloads are serialized to JSON. Circular references will fail.
- **Binary data**: Not supported through the bridge. Use file paths instead (e.g., return `/path/to/image.png` and load via `<img src="file:///path/to/image.png">`).

### View Limitations

- **Named views only**: Views must be registered by name in `coconut.views()`. No anonymous views.
- **View switching is instant**: No transition animations built-in. Use CSS transitions in your frontend.
- **No view props in v0.1**: `ctx:show(name, props)` is defined in the spec but not fully implemented.

### Bridge Limitations

- **Pre-ready buffering**: Events emitted before `coconut.ready()` are queued. Command calls wait for readiness.
- **Queue overflow**: If the event queue overflows before readiness, the runtime rejects with a `QueueOverflow` error.
- **No streaming**: Commands return a single value. No streaming responses.

### Platform Limitations

| Platform | Status | Notes |
|---|---|---|
| **macOS** | ✅ Full support | WKWebView, frameless, transparent, native dialogs |
| **Windows** | 🔲 Partial | WebView2 works, but `coconut://` and frameless are stubs |
| **Linux** | 🔲 Partial | WebKitGTK works, but `coconut://` and frameless are stubs |

---

## Next Steps

- Read the **[Concepts](./concepts.md)** page for architecture details
- Follow the **[Lua Backend Guide](./lua-guide.md)** for command patterns
- Check the **[API Reference](./api-reference.md)** for all functions
- See **[Examples](./examples.md)** for real-world projects
