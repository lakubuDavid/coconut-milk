# API Reference

Complete reference for all Lua and JavaScript APIs exposed by Coconut Milk.

---

## Lua API

### coconut.config(ctx)

**Signature:** `function coconut.config(ctx) → ctx`

**Description:** Required startup callback. Receives the runtime context and configures the application. Must return `ctx` (chainable).

**Example:**

```lua
function coconut.config(ctx)
  return ctx
    :setWindowSize({ w = 1280, h = 640 })
    :setMinimumWindowSize({ w = 800, h = 500 })
    :setTitle("My App")
    :setInitialView("home")
end
```

**Notes:**
- Called after `coconut.config.lua` is parsed
- Values set here **override** values from the config file
- Must return `ctx` for chainable usage

---

### coconut.views()

**Signature:** `function coconut.views() → table<string, ViewDescriptor>`

**Description:** Returns a table of named view descriptors. Keys are view names, values are view descriptors created by `View.load()`, `View.html()`, or `View.url()`.

**Example:**

```lua
function coconut.views()
  return {
    home = View.load("views/home.html")
      :on_load(function(ctx) print("Home loaded") end)
      :on_mount(function(ctx) print("Home visible") end),
    settings = View.load("views/settings.html"),
    about = View.html("<h1>About</h1>"),
    docs = View.url("https://example.com/docs"),
  }
end
```

**Notes:**
- View names must be unique strings
- View descriptors are lazily evaluated on first access
- Navigation uses view names, not descriptors

---

### coconut.commands(ctx)

**Signature:** `function coconut.commands(ctx) → nil`

**Description:** Optional callback for manual command registration. Called after auto-loaded commands from `commands/` folder.

**Example:**

```lua
function coconut.commands(ctx)
  ctx:bind("ping", function(params, ctx)
    return { message = "pong" }
  end)
end
```

**Notes:**
- Called after `commands/` folder is scanned
- Manual bindings with duplicate names will fail

---

### coconut.events(name, payload, ctx)

**Signature:** `function coconut.events(name: string, payload: table, ctx: CoconutContext) → nil`

**Description:** Global frontend event dispatcher. Called for every frontend-emitted event.

**Example:**

```lua
function coconut.events(name, payload, ctx)
  if name == "navigate" then
    ctx:show(payload.view)
  elseif name == "save_request" then
    local ok = save_data(payload.data)
    ctx:emit("save_response", { ok = ok })
  end
end
```

**Notes:**
- Called on the main Lua thread
- Events are dispatched synchronously
- Use `pcall` for safety if the handler may fail

---

### coconut.on_resize(ctx, w, h)

**Signature:** `function coconut.on_resize(ctx: CoconutContext, w: number, h: number) → nil`

**Description:** Called when the window is resized.

**Example:**

```lua
function coconut.on_resize(ctx, w, h)
  ctx:emit("window_resized", { w = w, h = h })
end
```

---

## Context Methods

All methods on `ctx` (the runtime context) are chainable unless noted.

### ctx:setWindowSize(size)

**Signature:** `ctx:setWindowSize({ w: number, h: number }) → ctx`

**Description:** Set the initial window size in pixels.

**Example:**

```lua
ctx:setWindowSize({ w = 1280, h = 640 })
```

---

### ctx:setMinimumWindowSize(size)

**Signature:** `ctx:setMinimumWindowSize({ w: number, h: number }) → ctx`

**Description:** Set the minimum window size for resizing.

**Example:**

```lua
ctx:setMinimumWindowSize({ w = 800, h = 500 })
```

---

### ctx:setMaximumWindowSize(size)

**Signature:** `ctx:setMaximumWindowSize({ w: number, h: number }) → ctx`

**Description:** Set the maximum window size for resizing.

**Example:**

```lua
ctx:setMaximumWindowSize({ w = 1920, h = 1080 })
```

---

### ctx:setMinimumWindowWidth(w)

**Signature:** `ctx:setMinimumWindowWidth(w: number) → ctx`

**Description:** Set the minimum window width only.

**Example:**

```lua
ctx:setMinimumWindowWidth(600)
```

---

### ctx:setMinimumWindowHeight(h)

**Signature:** `ctx:setMinimumWindowHeight(h: number) → ctx`

**Description:** Set the minimum window height only.

**Example:**

```lua
ctx:setMinimumWindowHeight(400)
```

---

### ctx:setMaximumWindowWidth(w)

**Signature:** `ctx:setMaximumWindowWidth(w: number) → ctx`

**Description:** Set the maximum window width only.

**Example:**

```lua
ctx:setMaximumWindowWidth(1600)
```

---

### ctx:setMaximumWindowHeight(h)

**Signature:** `ctx:setMaximumWindowHeight(h: number) → ctx`

**Description:** Set the maximum window height only.

**Example:**

```lua
ctx:setMaximumWindowHeight(900)
```

---

### ctx:setTitle(title)

**Signature:** `ctx:setTitle(title: string) → ctx`

**Description:** Set the window title.

**Example:**

```lua
ctx:setTitle("My Application")
```

---

### ctx:setResizable(resizable)

**Signature:** `ctx:setResizable(resizable: boolean) → ctx`

**Description:** Enable or disable window resizing.

**Example:**

```lua
ctx:setResizable(false)  -- Fixed-size window
```

---

### ctx:setFrameless(frameless)

**Signature:** `ctx:setFrameless(frameless: boolean) → ctx`

**Description:** Remove window chrome (title bar, borders). macOS only.

**Example:**

```lua
ctx:setFrameless(true)
```

**Notes:**
- On macOS, the title bar area becomes part of the window content area
- Traffic light buttons are hidden automatically
- Not supported on Windows/Linux (stub in v0.1)

---

### ctx:setTransparent(transparent)

**Signature:** `ctx:setTransparent(transparent: boolean) → ctx`

**Description:** Enable transparent window background. macOS only.

**Example:**

```lua
ctx:setTransparent(true)
ctx:setBackgroundColor(0, 0, 0, 0)  -- Fully transparent
```

**Notes:**
- When `transparent` is true, the frontend receives a `transparent-window` CSS class on `<body>`
- Not supported on Windows/Linux (stub in v0.1)

---

### ctx:setBackgroundColor(r, g, b, a)

**Signature:** `ctx:setBackgroundColor(r: number, g: number, b: number, a: number) → ctx`

**Description:** Set the window background color. Values are 0.0-1.0.

**Example:**

```lua
ctx:setBackgroundColor(0.07, 0.11, 0.1, 1.0)  -- Dark green (#121c1a)
```

---

### ctx:setInitialView(name)

**Signature:** `ctx:setInitialView(name: string) → ctx`

**Description:** Set the view to show at startup. Must match a key in `coconut.views()`.

**Example:**

```lua
ctx:setInitialView("home")
```

**Notes:**
- If the view name is not found, a warning is logged at startup
- View must be registered in `coconut.views()`

---

### ctx:bind(name, fn)

**Signature:** `ctx:bind(name: string, fn: CoconutCommandFn) → ctx`

**Description:** Register a command handler. One name maps to one handler.

**Parameters:**
- `name` (string): Command name, used by `coconut.call(name)`
- `fn` (function): Handler with signature `function(params, ctx)`

**Example:**

```lua
ctx:bind("greet", function(params, ctx)
  local name = params.name or "World"
  return { greeting = "Hello, " .. name .. "!" }
end)
```

**Errors:**
- `DuplicateCommand` — If a command with this name is already registered

---

### ctx:emit(name, payload)

**Signature:** `ctx:emit(name: string, payload: table) → nil`

**Description:** Emit an event to the frontend asynchronously. Queue-based delivery.

**Example:**

```lua
ctx:emit("toast", { message = "Saved successfully!", type = "success" })
```

**Notes:**
- Async — returns immediately
- Queued if bridge is not ready
- Preserves order per context

---

### ctx:emit_sync(name, payload)

**Signature:** `ctx:emit_sync(name: string, payload: table) → nil`

**Description:** Emit an event to the frontend synchronously. Blocks until delivered.

**Example:**

```lua
ctx:emit_sync("critical", { reason = "data loss risk" })
```

**Notes:**
- Blocking — does not return until dispatch is complete
- May fail if bridge is not ready

---

### ctx:show(name)

**Signature:** `ctx:show(name: string) → nil`

**Description:** Switch to a view by name.

**Example:**

```lua
ctx:show("settings")
```

**Notes:**
- Triggers `on_unmount` on the current view
- Triggers `on_mount` on the new view
- View must be registered in `coconut.views()`

---

### ctx:reload()

**Signature:** `ctx:reload() → nil`

**Description:** Reload the current active view.

**Example:**

```lua
ctx:reload()
```

---

### ctx:close()

**Signature:** `ctx:close() → nil`

**Description:** Request application or window shutdown.

**Example:**

```lua
ctx:close()
```

---

## View Factories

### View.load(path)

**Signature:** `View.load(path: string) → ViewDescriptor`

**Description:** Create a view descriptor from a local HTML file. Path is resolved relative to the app root.

**Example:**

```lua
local home = View.load("views/home.html")
```

---

### View.html(html)

**Signature:** `View.html(html: string) → ViewDescriptor`

**Description:** Create a view descriptor from an inline HTML string.

**Example:**

```lua
local about = View.html("<h1>About</h1><p>Version 1.0</p>")
```

**Notes:**
- HTML is written to a temp file and navigated via `file://`
- This ensures the navigation policy delegate fires for sub-resources
- Base URL is set correctly for `coconut://` asset resolution

---

### View.url(url)

**Signature:** `View.url(url: string) → ViewDescriptor`

**Description:** Create a view descriptor for an external URL.

**Example:**

```lua
local docs = View.url("https://example.com/docs")
```

**Notes:**
- External URLs are subject to the navigation policy
- Allow-listed URLs: `file://`, `coconut://`, `about:`, `data:`, `blob:`, localhost
- Non-allow-listed URLs open in the system browser

---

## View Lifecycle Methods

All methods are chainable on the view descriptor.

### view:on_load(fn)

**Signature:** `view:on_load(fn: function(ctx)) → view`

**Description:** Called once when the view is first created.

---

### view:on_mount(fn)

**Signature:** `view:on_mount(fn: function(ctx)) → view`

**Description:** Called every time the view becomes visible.

---

### view:on_unmount(fn)

**Signature:** `view:on_unmount(fn: function(ctx)) → view`

**Description:** Called every time the view is hidden.

---

### view:on_frontend_event(name, fn)

**Signature:** `view:on_frontend_event(name: string, fn: function(name, payload, ctx)) → view`

**Description:** Called when a frontend event is emitted while this view is active.

---

## JavaScript API

### coconut.ready()

**Signature:** `await coconut.ready(): Promise<void>`

**Description:** Returns a Promise that resolves when the bridge is ready. **Always call this first.**

**Example:**

```js
await coconut.ready()
console.log('Bridge is ready')
```

---

### coconut.call()

**Signature:** `await coconut.call<TResponse, TPayload>(name: TCommandName, payload?: TPayload): Promise<TResponse>`

**Description:** Call a Lua command. Returns a Promise that resolves with the command's return value.

**Parameters:**
- `name` (string): Registered command name
- `payload` (object, optional): Payload table passed to the Lua command handler

**Example:**

```js
const result = await coconut.call("greet", { name: "Ada" })
console.log(result.greeting)  // "Hello, Ada!"
```

**Error handling:**

```js
try {
  const result = await coconut.call("greet", { name: "Ada" })
} catch (err) {
  console.error(`${err.code}: ${err.message}`)
}
```

---

### coconut.emit()

**Signature:** `await coconut.emit(name: string, payload?: object): Promise<void>`

**Description:** Emit an event to the Lua backend.

**Example:**

```js
await coconut.emit("navigate", { view: "settings" })
```

---

### coconut.on()

**Signature:** `coconut.on(name: string, fn: (payload: object) => void): () => void`

**Description:** Register a listener for Lua-emitted events. Returns an unsubscribe function.

**Example:**

```js
const unsub = coconut.on("toast", (payload) => {
  console.log(payload.message)
})

// Later:
unsub()
```

---

### coconut.views()

**Signature:** `await coconut.views(): Promise<string[]>`

**Description:** Returns the list of registered view names.

**Example:**

```js
const views = await coconut.views()
// ["home", "settings", "about"]
```

---

### coconut.ping()

**Signature:** `await coconut.ping(): Promise<string>`

**Description:** Connectivity test. Returns `"pong"`.

**Example:**

```js
const pong = await coconut.ping()
// "pong"
```

---

### coconut.window

#### coconut.window.minimize()

**Signature:** `await coconut.window.minimize(): Promise<void>`

**Description:** Minimize the window to the dock/taskbar.

**Example:**

```js
await coconut.window.minimize()
```

---

#### coconut.window.toggleFullscreen()

**Signature:** `await coconut.window.toggleFullscreen(): Promise<void>`

**Description:** Toggle fullscreen mode.

**Example:**

```js
await coconut.window.toggleFullscreen()
```

---

#### coconut.window.close()

**Signature:** `await coconut.window.close(): Promise<void>`

**Description:** Close the window / quit the application.

**Example:**

```js
await coconut.window.close()
```

---

### coconut.fs

#### coconut.fs.readText()

**Signature:** `await coconut.fs.readText(path: string): Promise<{ ok: boolean; data?: string; error?: string }>`

**Description:** Read a text file from the filesystem.

**Parameters:**
- `path` (string): Absolute path to the file

**Example:**

```js
const { ok, data, error } = await coconut.fs.readText("/path/to/file.txt")
if (ok) {
  console.log(data)  // File contents
} else {
  console.error(error)
}
```

---

## Error Types

### CoconutError (JavaScript)

```typescript
interface CoconutError {
  code: string       // Error code (e.g., "CommandNotFound")
  message: string    // Human-readable description
  details?: unknown  // Optional additional context
}
```

### CoconutBridgeError (Lua)

```lua
---@class CoconutBridgeError
---@field code string
---@field message string
---@field details? table
```

---

## Next Steps

- See **[Examples](./examples.md)** for real-world usage patterns
- Read **[Troubleshooting](./troubleshooting.md)** for common errors and solutions
