# Coconut Milk Framework — Agent Skill

Cross-platform desktop app framework. **Lua backend** + **HTML/CSS/JS frontend** + **native webview**. Single ~2MB binary, no Node.js or Chromium needed.

---

## 1. Project Structure

```
my-app/
├── main.lua               # Entry point: config + views
├── coconut.config.lua     # Optional defaults (JSON-compatible table)
├── coconut.d.ts           # Generated TypeScript type defs
├── tsconfig.json          # JS/TS type-checking config
├── commands/              # Backend logic (auto-loaded)
│   ├── example.lua        # Lua command with @command annotations
│   └── example.g.js       # Generated JS wrapper
├── views/                 # Frontend HTML views
│   ├── index.html
│   ├── app.js
│   └── style.css
└── assets/                # Static assets (served via coconut://)
```

**How the binary loads your app:**

```
coconut                     # Uses current directory as project root
coconut --root /path/to/app # Explicit root
coconut --debug             # Verbose logging
```

The binary reads `main.lua` from project root → calls `coconut.config(ctx)` → `coconut.views()` → auto-loads `commands/*.lua` → creates webview → runs event loop.

---

## 2. Lua API (Backend)

### 2.1 Config Callback (Required)

Every app MUST define `coconut.config(ctx)`. Must return `ctx` for chaining:

```lua
function coconut.config(ctx)
  return ctx
    :setWindowSize({ w = 1280, h = 640 })
    :setMinimumWindowSize({ w = 800, h = 500 })
    :setTitle("My App")
    :setResizable(true)
    :setInitialView("home")
end
```

**Available chainable context methods:**

| Method | Signature | Description |
|--------|-----------|-------------|
| `setWindowSize` | `({w, h}) -> ctx` | Initial window size in px |
| `setMinimumWindowSize` | `({w, h}) -> ctx` | Min resize size |
| `setMaximumWindowSize` | `({w, h}) -> ctx` | Max resize size |
| `setTitle` | `(string) -> ctx` | Window title |
| `setResizable` | `(bool) -> ctx` | Allow resizing |
| `setFrameless` | `(bool) -> ctx` | Remove window chrome (macOS only) |
| `setTransparent` | `(bool) -> ctx` | Transparent BG (macOS only) |
| `setBackgroundColor` | `(r, g, b, a) -> ctx` | RGBA 0.0–1.0 scale |
| `setInitialView` | `(string) -> ctx` | View to show at startup |

### 2.2 Views Callback

Define your app's views:

```lua
function coconut.views()
  return {
    home = View.load("views/home.html"),
    settings = View.load("views/settings.html"),
    about = View.html("<h1>About</h1>"),              -- Inline HTML
    external = View.url("https://example.com"),        -- External URL
  }
end
```

**View factories:**

| Factory | Description | Notes |
|---------|-------------|-------|
| `View.load(path)` | Local HTML file | Relative to project root |
| `View.html(string)` | Inline HTML string | Written to temp file, served via `file://` |
| `View.url(string)` | External URL | Opens in webview (allow-listed) or system browser |

**View lifecycle callbacks (chainable):**

```lua
View.load("views/home.html")
  :on_load(function(ctx)
    -- Called ONCE when view is first created
  end)
  :on_mount(function(ctx)
    -- Called EVERY TIME view becomes visible
  end)
  :on_unmount(function(ctx)
    -- Called EVERY TIME view is hidden
  end)
  :on_frontend_event("navigate", function(name, payload, ctx)
    -- Called when frontend emits this event while view is active
  end)
```

### 2.3 Commands

The backend logic. Commands are registered in two ways:

**A. Auto-loaded from `commands/*.lua`:**

```lua
-- commands/greet.lua
---@command greet
---@param params { name?: string }
---@return { greeting: string }
local function greet(params, ctx)
  local name = params.name or "World"
  return { greeting = "Hello, " .. name .. "!" }
end

return { greet = greet }
```

Every file in `commands/` is loaded and registered automatically. The `---@command` annotation is consumed by `coconut generate` for type generation.

**B. Manual registration in `coconut.commands(ctx)`:**

```lua
function coconut.commands(ctx)
  ctx:bind("ping", function(params, ctx)
    return { message = "pong" }
  end)
end
```

Manual bindings run AFTER auto-loaded commands. Duplicate names throw `DuplicateCommand`.

**Command handler signature:** `function params, ctx -> any`

- `params` — Lua table from JSON payload (always a table)
- `ctx` — Runtime context (same object as config)

**Return values:**
- `table` → JSON object
- 1-indexed sequential table → JSON array
- `string`, `number`, `boolean` → corresponding JSON
- `nil` or no return → JSON `null`/JS `undefined`

### 2.4 Context Methods (Runtime)

Available inside command handlers and lifecycle callbacks:

```lua
ctx:emit("event_name", { key = "value" })          -- Async, queue-based
ctx:emit_sync("event_name", { key = "value" })     -- Blocking
ctx:show("view_name")                                -- Switch view
ctx:reload()                                         -- Reload current view
ctx:close()                                          -- Quit application
```

### 2.5 Global Event Dispatcher

```lua
function coconut.events(name, payload, ctx)
  if name == "navigate" then
    ctx:show(payload.view)
  elseif name == "save_request" then
    -- Handle frontend event
  end
end
```

### 2.6 Window Resize Hook

```lua
function coconut.on_resize(ctx, w, h)
  ctx:emit("window_resized", { w = w, h = h })
end
```

### 2.7 Logging

```lua
coconut.info("message")    -- [INFO] cyan
coconut.warn("message")    -- [WARN] yellow
coconut.error("message")   -- [ERROR] red
coconut.debug("message")   -- [DEBUG] grey (only shown with --debug)
```

### 2.8 Filesystem (Lua)

These are **not exported via `ctx`** — they're global functions loaded by Coconut:

```lua
coconut.fs.readText(path)     -> string or nil
coconut.fs.writeText(path, content) -> boolean
coconut.fs.exists(path)       -> boolean
coconut.fs.listDir(path)      -> array of {name, path, is_dir}
```

Note: `listDir` returns results sorted directories-first, both directories and files alphabetically.

### 2.9 Native Dialogs (Lua)

```lua
-- Open file dialog
local result = coconut.dialog.open("Title", multi_select, choose_directory)
-- result = { confirmed, path, is_dir, paths }

-- Save file dialog
local result = coconut.dialog.save("Title", "filename.ext")
-- result = { confirmed, path }

-- Message box
local result = coconut.dialog.messageBox("Title", "Message", "info")
-- Types: "info", "warning", "error", "question"
```

**Always wrap dialog calls in `pcall` for crash safety:**

```lua
local ok, result = pcall(coconut.dialog.open, "Open File", false, true)
if not ok then
  coconut.error("Dialog failed: " .. tostring(result))
  return { cancelled = true }
end
```

---

## 3. JavaScript API (Frontend)

The `coconut` global object is injected into the webview at `document_start`. It's available before any page scripts run.

### 3.1 Bridge Readiness

```js
await coconut.ready()
// Always call this first — waits for bridge handshake
```

### 3.2 Call a Lua Command

```js
const result = await coconut.call("greet", { name: "Ada" })
// result = { greeting: "Hello, Ada!" }
```

Error handling:

```js
try {
  const result = await coconut.call("greet", { name: "Ada" })
} catch (err) {
  console.error(`${err.code}: ${err.message}`)
  // err.code = "CommandNotFound" | "LuaError" | "NotReady" | etc.
}
```

### 3.3 Emit Events to Lua

```js
await coconut.emit("navigate", { view: "settings" })
```

### 3.4 Listen for Lua Events

```js
const unsub = coconut.on("toast", (payload) => {
  showToast(payload.message, payload.type)
})
// Later:
unsub()  // Unsubscribe
```

### 3.5 Get View Names

```js
const views = await coconut.views()
// ["home", "settings", "about"]
```

### 3.6 Connectivity Test

```js
const pong = await coconut.ping()
// "pong"
```

### 3.7 Window Controls

```js
await coconut.window.minimize()
await coconut.window.toggleFullscreen()
await coconut.window.close()
```

### 3.8 Filesystem

```js
const { ok, data, error } = await coconut.fs.readText("/path/to/file.txt")
if (ok) console.log(data)
else console.error(error)
```

---

## 4. The `coconut://` Scheme

Assets in views should use `coconut://` for portable, filesystem-independent paths:

```html
<link rel="stylesheet" href="coconut://assets/style.css">
<script src="coconut://assets/app.js"></script>
<img src="coconut://assets/icon.png">
```

**Resolution:** `coconut://assets/style.css` → `{project_root}/assets/style.css`

**View navigation via `coconut://`:**
```html
<a href="coconut://settings">Settings</a>
<!-- Triggers Lua: coconut.emit('navigate', { view: 'settings' }) -->
```

**Platform support:** macOS only (WKWebView `WKURLSchemeHandler`). Windows/Linux are stubs.

---

## 5. Code Generation (`coconut generate`)

Scans `commands/*.lua` for `@command` annotations and generates typed wrappers.

```bash
coconut generate
```

**Annotation format:**

```lua
---@command <name>
---@param params { <field>: <type>, ... }
---@return { <field>: <type>, ... }
```

**Generated output (in `generated/`):**

| File | Content |
|------|---------|
| `example.g.lua` | Lua registration glue (auto-loaded) |
| `example.d.ts` | TypeScript type definitions |
| `example.g.js` | JavaScript wrapper function |
| `commands.d.ts` | Union type `CoconutCommandName = "cmd1" \| "cmd2"` |

Also writes `coconut.d.ts` at project root (ambient `window.coconut` types).

---

## 6. The Bridge Protocol (Conceptual)

**Frontend → Backend:** `coconut.call(name, payload)` → RPC envelope → Lua handler → response

**Backend → Frontend:** `ctx:emit(name, payload)` → queue → injected JS dispatches to `coconut.on()` listeners

**Readiness handshake:** Frontend sends `{ type: "ready" }` when bridge initializes. Before that:
- `coconut.call()` waits (Promise stays pending)
- `coconut.emit()` queues events (delivered after ready)
- `ctx:emit()` queues events (delivered after ready)
- If queue overflows: `QueueOverflow` error

**Payload limitations:**
- Must be tables/objects (no raw strings or primitives as payload)
- No binary data through the bridge — use file paths instead
- Circular references fail in JSON serialization

---

## 7. Common Patterns

### 7.1 State Management (Lua)

```lua
local state = {
  theme = "dark",
  precision = 2,
}

local function save(params, ctx)
  if params.theme then state.theme = params.theme end
  return { ok = true }
end

local function load(_, ctx)
  return state
end
```

### 7.2 Error Handling (Lua)

```lua
local function safe_op(params, ctx)
  local ok, result = pcall(function()
    return coconut.fs.readText(params.path)
  end)
  if not ok then
    return { error = "Failed: " .. tostring(result) }
  end
  return { data = result }
end
```

### 7.3 Image Preview (no binary through bridge)

```lua
-- Lua: return file path
local function read_file(params, ctx)
  return { type = "image", path = "/abs/path/to/image.png" }
end
```

```js
// JS: render via file://
const result = await coconut.call("read_file", { path })
if (result.type === "image") {
  img.src = `file://${result.path}`
}
```

### 7.4 Keyboard Shortcuts (JS)

```js
document.addEventListener("keydown", (e) => {
  if ((e.metaKey || e.ctrlKey) && e.key === "s") {
    e.preventDefault()
    coconut.call("save_file", { content: editor.getValue() })
  }
})
```

### 7.5 Event-Driven Pattern

```lua
-- Lua: emit event, frontend shows toast
local function save_file(params, ctx)
  write_to_disk(params.path, params.content)
  ctx:emit("file_saved", { path = params.path, ok = true })
  return { ok = true }
end
```

```js
// JS: listen for events
coconut.on("file_saved", (p) => showToast(`Saved: ${p.path}`))
```

---

## 8. Key Gotchas

| # | Gotcha | Explanation |
|---|--------|-------------|
| 1 | **`coconut.ready()` first** | Calling `coconut.call()` before bridge is ready returns `NotReady` error |
| 2 | **ESM modules fail from `coconut://`** | `type="module"` scripts are CORS-blocked when page loads from `file://`. Use IIFE bundles + plain `<script>` tags |
| 3 | **No binary data in bridge** | Return file paths instead of binary payloads |
| 4 | **Table-only payloads** | `ctx:emit("ev", "raw")` fails — use `ctx:emit("ev", { msg = "raw" })` |
| 5 | **`View.html()` uses temp files** | Inline HTML is written to a temp file and served via `file://` URL. `View.url()` navigates the webview directly |
| 6 | **`coconut://` is macOS-only** | Scheme handler not implemented on Windows/Linux |
| 7 | **Frameless/transparent macOS-only** | Not available on Windows/Linux |
| 8 | **Dialog crashes** | Always use `pcall` around `coconut.dialog.*` calls |
| 9 | **Commands run synchronously** | No async/await in Lua. Long operations block the UI |
| 10 | **Single window only** | Use `ctx:show()` for multi-page apps, not multiple windows |
| 11 | **Duplicate command names fail** | Auto-loaded + manual `ctx:bind()` with same name throws error |
| 12 | **`@command` annotation is required for generation** | Without it, `coconut generate` skips the function |
| 13 | **Lua paths must end with `;`** | When manipulating `package.path`, ensure there's a trailing `;` or it breaks |

---

## 9. Key Constraints

- **Everything is synchronous in Lua** — no threading, no async/await. Long operations block.
- **The `coconut` object is injected at document_start** — available to all scripts.
- **Generated files (`.g.lua`, `.g.js`, `.d.ts`) are auto-generated** — don't edit them manually.
- **`coconut.d.ts` must have `export {}`** at the top to act as a module for ambient types.
- **View names must be unique strings** — used for routing (`ctx:show()`), navigation links (`coconut://view_name`), and lifecycle callbacks.
- **Project root = CWD** — not the binary's location. All paths resolve relative to the current working directory.

---

## 10. Quick Reference

### Startup Lifecycle

```
1. Binary starts → reads coconut.config.lua (optional)
2. Runs main.lua
3. Calls coconut.config(ctx)  → must return ctx
4. Calls coconut.views()      → returns view table
5. Auto-loads commands/*.lua
6. Calls coconut.commands(ctx) (optional, manual bindings)
7. Creates webview, loads initial view
8. Injects coconut JS API
9. Frontend sends "ready" → bridge active
10. Main loop: dispatch RPC, handle events, sleep
```

### Log Levels

| Config value | Shows |
|---|---|
| `"debug"` | All messages |
| `"info"` | `[INFO]`, `[WARN]`, `[ERROR]` (default) |
| `"warn"` | `[WARN]`, `[ERROR]` |
| `"error"` | Only `[ERROR]` |

### Config File Format

```lua
return {
  window_width = 1280,
  window_height = 640,
  initial_view = "home",
  title = "My App",
  frameless = false,
  transparent = false,
  resizable = true,
  debug = {
    enabled = true,
    showTransportDump = false,
    logLevel = "info",
  },
  generators = {
    output_dir = "generated",
  },
}
```
