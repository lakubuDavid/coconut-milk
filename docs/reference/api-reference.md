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

### coconut.events(event)

**Signature:** `function coconut.events(event: CoconutEvent) → nil`

**Description:** Last-resort fallback in the event dispatch chain.
Fires after all `view:on()` and `coconut.on()` callbacks.

**Example:**

```lua
function coconut.events(event)
  if event.name == "navigate" then
    ctx:show(event.view)
  elseif event.name == "save_request" then
    local ok = save_data(event.data)
    ctx:emit({ name = "save_response", ok = ok })
  end
end
```

**Notes:**
- Fires after all other handlers
- The event carries all payload fields merged directly

---

### coconut.on(name, fn, opts?)

**Signature:** `coconut.on(name: string, fn: function(event: CoconutEvent), opts?: { once?: boolean }) → function`

**Description:** Register a global event listener. Returns an unsubscribe function.

**Example:**

```lua
local unsub = coconut.on("resize", function(event)
  print("resized to", event.w, event.h)
end)

-- One-time listener
coconut.on("ready", function(event)
  print("app ready")
end, { once = true })

-- Later:
unsub()
```

**Notes:**
- Multiple listeners for the same event fire in registration order (FIFO)
- Calling `event:stopPropagation()` prevents the event from reaching `coconut.events(event)`
- `{ once = true }` auto-unsubscribes after the first fire

---

### coconut.keybind(combo, handler, opts?)

**Signature:** `coconut.keybind(combo: string | table, handler: function | string, opts?: table) → function`

**Description:** Register a keyboard shortcut. Can bind to a function or command name. Returns an unregister function.

**Parameters:**
- `combo` (string | table): Key combination (e.g., `"mod+s"`) or platform-specific table `{mac = "cmd+s", win = "ctrl+s"}`
- `handler` (function | string): Function to call or command name to execute
- `opts` (table, optional): Options table with `id`, `scope`, `platform` fields

**Example:**

```lua
-- Simple keybind
coconut.keybind("mod+s", function()
  ctx:emit({ name = "save" })
end, { id = "editor.save", scope = "editor" })

-- Bind to a command name
coconut.keybind("mod+shift+p", "editor_palette", { id = "app.palette" })

-- Platform-specific combos
coconut.keybind({
  mac   = "cmd+s",
  win   = "ctrl+s",
  linux = "ctrl+s",
}, save_function)

-- Platform-level keybind (consumed before WebView sees it)
coconut.keybind("mod+q", quit_function, { platform = true })

-- Unregister later
local unreg = coconut.keybind("mod+s", fn)
unreg()
```

**Combo format:**
- Modifiers: `mod` (cmd on macOS, ctrl elsewhere), `cmd`, `ctrl`, `alt`, `shift`
- Keys: `a`-`z`, `0`-`9`, `enter`, `tab`, `space`, `escape`, `up`, `down`, `left`, `right`, `f1`-`f20`

**Options:**
- `id` (string): Unique identifier for overrides
- `scope` (string): `"global"` or view name (default: `"global"`)
- `platform` (boolean): If true, consumed at platform level before WebView

**Notes:**
- `mod` maps to `cmd` on macOS, `ctrl` on Windows/Linux
- Platform keybinds prevent system beep on macOS
- Returns unregister function

---

### coconut.args

**Signature:** `coconut.args: table`

**Description:** Read-only table containing parsed CLI arguments passed to the application.

**Structure:**

```lua
coconut.args = {
  positional = {"arg1", "arg2"},  -- Positional arguments
  named = {mode = "dev", port = "3000"},  -- Named arguments (--key=value)
  flags = {verbose = true, debug = false}  -- Boolean flags (--flag)
}
```

**Example:**

```lua
-- App started with: coconut --verbose --mode=dev myfile.lua
function coconut.config(ctx)
  local file = coconut.args.positional[1]  -- "myfile.lua"
  local mode = coconut.args.named.mode     -- "dev"
  local verbose = coconut.args.flags.verbose  -- true
  
  if verbose then
    coconut.info("Verbose mode enabled")
  end
  
  return ctx
end
```

**Notes:**
- Read-only — cannot modify at runtime
- Available in both Lua and JavaScript
- Parsed automatically from `argv`

---

### coconut.env

**Signature:** `coconut.env: table`

**Description:** Lazy-loading table for environment variables. Access via `coconut.env.VAR_NAME`.

**Special keys:**
- `coconut.env.cwd` — Current working directory
- `coconut.env.homedir` — User home directory
- `coconut.env.pathSeparator` — Platform path separator (`/` or `\`)

**Example:**

```lua
local home = coconut.env.HOME  -- or coconut.env.USERPROFILE on Windows
local cwd = coconut.env.cwd
local sep = coconut.env.pathSeparator

print("Home:", home)
print("CWD:", cwd)
print("Separator:", sep)

-- Access any environment variable
local path = coconut.env.PATH
if path then
  print("PATH:", path)
end
```

**Notes:**
- Uses metatable `__index` for lazy evaluation
- Returns `nil` if variable doesn't exist
- Platform-specific variables work (HOME on Unix, USERPROFILE on Windows)

---

### coconut.json

**Signature:** `coconut.json: table`

**Description:** JSON serialization utilities using nlohmann::json under the hood.

**Methods:**
- `jsonify(table)` — Convert Lua table to JSON string
- `parse(string)` — Parse JSON string to Lua table

**Example:**

```lua
local data = {name = "Ada", age = 30}
local json_str = coconut.json.jsonify(data)
print(json_str)  -- {"age":30,"name":"Ada"}

local parsed = coconut.json.parse(json_str)
print(parsed.name)  -- "Ada"

-- Handle parse errors
local invalid = coconut.json.parse("not json")
if invalid == nil then
  print("Parse failed")
end
```

**Notes:**
- Handles nested tables and arrays
- Returns `nil` on parse failure
- Arrays are 1-indexed Lua tables

---

### coconut.clipboard

**Signature:** `coconut.clipboard: table`

**Description:** Read and write text to the system clipboard.

**Methods:**
- `readText()` — Read text from clipboard
- `writeText(text)` — Write text to clipboard

**Example:**

```lua
-- Write to clipboard
local ok = coconut.clipboard.writeText("Hello, clipboard!")
if ok then
  print("Copied to clipboard")
end

-- Read from clipboard
local text = coconut.clipboard.readText()
if text then
  print("Clipboard contains:", text)
end
```

**Notes:**
- Returns boolean success for `writeText`
- Returns `nil` if clipboard is empty or unavailable
- Text only — no binary data support

---

### coconut.openUrl(url)

**Signature:** `coconut.openUrl(url: string) → boolean`

**Description:** Open a URL in the system default browser.

**Example:**

```lua
local ok = coconut.openUrl("https://example.com")
if ok then
  print("Opened in browser")
else
  print("Failed to open URL")
end
```

**Notes:**
- Returns `true` on success, `false` on failure
- Uses platform-specific handlers (`open` on macOS, `xdg-open` on Linux)
- Supports `http://`, `https://`, `mailto:`, `file://` schemes

---

### coconut.notify(title, body)

**Signature:** `coconut.notify(title: string, body: string) → boolean`

**Description:** Show a native system notification.

**Example:**

```lua
local ok = coconut.notify("Build Complete", "Your app compiled successfully")
if ok then
  print("Notification shown")
end
```

**Notes:**
- Returns `true` on success, `false` on failure
- Platform-specific implementation (NSUserNotification on macOS)
- May require user permission on some platforms

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

### ctx:emit(event)

**Signature:** `ctx:emit(event: CoconutEvent) → nil`

**Description:** Dispatch an event to both the Lua dispatch chain
and the frontend. Accepts a single event table.

**Example:**

```lua
ctx:emit({ name = "toast", message = "Saved successfully!", type = "success" })
```

**Notes:**
- Runs the full Lua dispatch chain (view → subscribe → fallback)
- Forwards to JS via bridge
- Async — returns immediately

---

### ctx:emit_sync(event)

**Signature:** `ctx:emit_sync(event: CoconutEvent) → nil`

**Description:** Synchronous version of `ctx:emit()`. Blocks until delivered.

**Example:**

```lua
ctx:emit_sync({ name = "critical", reason = "data loss risk" })
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

### view:on(name, fn)

**Signature:** `view:on(name: string, fn: function(event: CoconutEvent)) → view`

**Description:** Register a view-scoped event listener. Fires when an event is emitted while this view is active, before global handlers.

---

## JavaScript API

### CoconutEvent class

All events in JS are instances of `CoconutEvent`:

```ts
class CoconutEvent {
  readonly name: string           // event name
  get type(): string              // proxies to name
  readonly target: string         // view name or ""
  defaultPrevented: boolean       // set by preventDefault()
  propagationStopped: boolean     // set by stopPropagation()

  preventDefault(): void          // sets defaultPrevented = true
  stopPropagation(): void         // prevents further handlers
  stopImmediatePropagation(): void // both of the above
}
```

**Properties:**

| Property | Type | Description |
|---|---|---|
| `name` | `string` | Event name (e.g., `"resize"`, `"ready"`, `"close"`) |
| `type` | `string` | Getter that returns `name` (DOM compatibility) |
| `target` | `string` | View name where event originated, or `""` for global events |
| `defaultPrevented` | `boolean` | `true` if `preventDefault()` was called |
| `propagationStopped` | `boolean` | `true` if `stopPropagation()` was called |
| `...payload` | `any` | All payload fields merged directly into the event object |

**Methods:**

#### preventDefault()

**Signature:** `event.preventDefault(): void`

**Description:** Marks the event as cancelled. Used for cancellable events like `close`.

**Example:**

```js
coconut.on("close", (event) => {
  if (hasUnsavedChanges) {
    event.preventDefault()  // Veto the close
    showSaveDialog()
  }
})
```

**Notes:**
- Sets `defaultPrevented = true`
- Does NOT stop propagation to other handlers
- Only meaningful for cancellable events (currently: `close`)

---

#### stopPropagation()

**Signature:** `event.stopPropagation(): void`

**Description:** Prevents the event from reaching subsequent tiers in the dispatch chain.

**Example:**

```js
coconut.on("save", (event) => {
  saveFile()
  event.stopPropagation()  // Don't reach fallback handler
})
```

**Notes:**
- Sets `propagationStopped = true`
- Stops event from reaching next tier (view → subscribe → fallback)
- Does NOT prevent other handlers in the same tier from firing

---

#### stopImmediatePropagation()

**Signature:** `event.stopImmediatePropagation(): void`

**Description:** Stops propagation AND prevents default. Combination of both methods.

**Example:**

```js
coconut.on("critical", (event) => {
  handleCriticalEvent(event)
  event.stopImmediatePropagation()  // Stop everything
})
```

**Notes:**
- Sets both `defaultPrevented = true` and `propagationStopped = true`
- Strongest form of event cancellation

---

### Event Object Shape (Lua)

In Lua, events are tables with a metatable providing methods:

```lua
local event = {
  name = "resize",
  target = "editor",
  defaultPrevented = false,
  propagationStopped = false,
  w = 1024,  -- payload field
  h = 768,   -- payload field
}

-- Methods via metatable:
event:preventDefault()
event:stopPropagation()
event:stopImmediatePropagation()

-- Type getter via metatable __index:
print(event.type)  -- "resize" (same as event.name)
```

**Payload merging:** All payload fields are merged directly into the event table:

```lua
ctx:emit({ name = "toast", message = "Saved!", duration = 3000 })

-- In handler:
coconut.on("toast", function(event)
  print(event.name)      -- "toast"
  print(event.message)   -- "Saved!"
  print(event.duration)  -- 3000
end)
```

---

### Lifecycle Events

Coconut emits lifecycle events at key moments. Subscribe via `coconut.on()` or `view:on()`.

#### resize

**When:** Window is resized by the user or programmatically.

**Payload:**
```lua
{
  name = "resize",
  w = 1024,  -- new width in pixels
  h = 768    -- new height in pixels
}
```

**Example:**

```lua
coconut.on("resize", function(event)
  print(string.format("Resized to %dx%d", event.w, event.h))
  -- Reflow UI, update canvas size, etc.
end)

-- View-scoped
local editor = View.load("editor.html")
  :on("resize", function(event)
    reflowEditor(event.w, event.h)
  end)
```

**Notes:**
- Fired on macOS via NSWindow delegate
- May fire rapidly during resize drag
- `target` is the active view name

---

#### ready

**When:** App is initialized, after initial view mounts, before event loop starts.

**Payload:**
```lua
{
  name = "ready"
}
```

**Example:**

```lua
coconut.on("ready", function(event)
  print("App ready!")
  -- Initialize state, load data, etc.
end, { once = true })
```

**Notes:**
- Fires once per app lifecycle
- Bridge is ready, commands can be called
- Good place for one-time initialization
- Use `{ once = true }` to auto-unsubscribe

---

#### close

**When:** App is about to close (user clicked close button, called `ctx:close()`, etc.).

**Payload:**
```lua
{
  name = "close"
}
```

**Cancellable:** Yes — call `event:preventDefault()` to veto the close.

**Example:**

```lua
coconut.on("close", function(event)
  if hasUnsavedChanges() then
    event:preventDefault()  -- Cancel close
    promptSaveBeforeQuit()
  end
end)
```

**Notes:**
- Only cancellable lifecycle event
- If `defaultPrevented` is true, app stays open
- Webview is still alive during this handler
- Use for cleanup, save prompts, confirmation dialogs

---

#### focus

**When:** Window gains focus (becomes active window).

**Payload:**
```lua
{
  name = "focus",
  active = true
}
```

**Example:**

```lua
coconut.on("focus", function(event)
  print("Window focused")
  -- Resume animations, poll for updates, etc.
end)
```

**Notes:**
- Fired on macOS via NSApplication delegate
- `active` field is always `true` for focus events
- Use for resuming activity when window becomes active

---

#### blur

**When:** Window loses focus (another window becomes active).

**Payload:**
```lua
{
  name = "blur",
  active = false
}
```

**Example:**

```lua
coconut.on("blur", function(event)
  print("Window blurred")
  -- Pause animations, save draft, etc.
end)
```

**Notes:**
- Fired on macOS via NSApplication delegate
- `active` field is always `false` for blur events
- Use for pausing activity when window loses focus

---

### Dispatch Chain Order

Events flow through three tiers in order:

```
1. View scope      → view:on(name, fn) fires if active view has listener
2. Global subscribe → coconut.on(name, fn) listeners fire in FIFO order
3. Fallback        → coconut.events(event) fires last
```

**Propagation control:**
- `event:stopPropagation()` — Skip to next event, don't reach subsequent tiers
- `event:preventDefault()` — Mark as cancelled (for cancellable events like `close`)
- `event:stopImmediatePropagation()` — Both of the above

**Example flow:**

```lua
-- Tier 1: View-scoped
local editor = View.load("editor.html")
  :on("save", function(event)
    print("Tier 1: view handler")
    -- event:stopPropagation() would skip tiers 2 and 3
  end)

-- Tier 2: Global subscribe
coconut.on("save", function(event)
  print("Tier 2: global handler 1")
end)

coconut.on("save", function(event)
  print("Tier 2: global handler 2")
end)

-- Tier 3: Fallback
function coconut.events(event)
  if event.name == "save" then
    print("Tier 3: fallback")
  end
end

-- Emit triggers all three tiers:
ctx:emit({ name = "save" })
-- Output:
-- Tier 1: view handler
-- Tier 2: global handler 1
-- Tier 2: global handler 2
-- Tier 3: fallback
```

---

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

**Signature:** `await coconut.emit(event: Record<string, unknown>): Promise<void>`

**Description:** Emit an event to the Lua backend. Accepts a single event object.

**Example:**

```js
await coconut.emit({ name: "navigate", view: "settings" })
```

---

### coconut.on()

**Signature:** `coconut.on(name: string, fn: (event: CoconutEvent) => void, opts?: { once?: boolean }): () => void`

**Description:** Register a listener for Lua-emitted events. Returns an unsubscribe function.
The event is a `CoconutEvent` object with all payload fields merged in.

**Example:**

```js
const unsub = coconut.on("toast", (event) => {
  console.log(event.message)
})

// One-time:
coconut.on("ready", () => { ... }, { once: true })

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

### coconut.keybind()

**Signature:** `coconut.keybind(combo: string | object, handler: function | string, opts?: object): () => void`

**Description:** Register a keyboard shortcut in JavaScript. Returns an unregister function.

**Parameters:**
- `combo` (string | object): Key combination (e.g., `"mod+s"`) or platform-specific object `{mac: "cmd+s", win: "ctrl+s"}`
- `handler` (function | string): Function to call or command name to execute
- `opts` (object, optional): Options object with `id`, `scope` fields

**Example:**

```js
// Simple keybind
coconut.keybind("mod+s", () => saveFile(), { id: "editor.save", scope: "editor" })

// Platform-specific combos
coconut.keybind({ mac: "cmd+s", win: "ctrl+s" }, () => saveFile())

// Returns unregister function
const unreg = coconut.keybind("mod+s", fn)
unreg()
```

**Combo format:**
- Modifiers: `mod` (cmd on macOS, ctrl elsewhere), `cmd`, `ctrl`, `alt`, `shift`
- Keys: `a`-`z`, `0`-`9`, `enter`, `tab`, `space`, `escape`, `up`, `down`, `left`, `right`, `f1`-`f20`

**Options:**
- `id` (string): Unique identifier for overrides
- `scope` (string): `"global"` or view name (default: `"global"`)

**Notes:**
- `mod` maps to `cmd` on macOS, `ctrl` on Windows/Linux
- JS keybinds fire before Lua keybinds in the dispatch chain
- Returns unregister function

---

### coconut.args

**Signature:** `coconut.args: object`

**Description:** Read-only object containing parsed CLI arguments passed to the application.

**Structure:**

```js
coconut.args = {
  positional: ["arg1", "arg2"],  // Positional arguments
  named: {mode: "dev", port: "3000"},  // Named arguments (--key=value)
  flags: {verbose: true, debug: false}  // Boolean flags (--flag)
}
```

**Example:**

```js
await coconut.ready()

// App started with: coconut --verbose --mode=dev myfile.lua
const file = coconut.args.positional[0]  // "myfile.lua"
const mode = coconut.args.named.mode     // "dev"
const verbose = coconut.args.flags.verbose  // true

if (verbose) {
  console.log("Verbose mode enabled")
}
```

**Notes:**
- Read-only — cannot modify at runtime
- Available after `coconut.ready()` resolves
- Mirrors the Lua-side `coconut.args` table

---

### coconut.clipboard

**Signature:** `coconut.clipboard: object`

**Description:** Read and write text to the system clipboard via bridge commands.

**Methods:**
- `readText(): Promise<string>` — Read text from clipboard
- `writeText(text: string): Promise<boolean>` — Write text to clipboard

**Example:**

```js
// Write to clipboard
const ok = await coconut.call("clipboard_write", { text: "Hello, clipboard!" })
if (ok) {
  console.log("Copied to clipboard")
}

// Read from clipboard
const text = await coconut.call("clipboard_read")
if (text) {
  console.log("Clipboard contains:", text)
}
```

**Notes:**
- Implemented via `clipboard_read` and `clipboard_write` commands
- Text only — no binary data support

---

### coconut.openUrl()

**Signature:** `await coconut.openUrl(url: string): Promise<boolean>`

**Description:** Open a URL in the system default browser.

**Example:**

```js
const ok = await coconut.call("openUrl", { url: "https://example.com" })
if (ok) {
  console.log("Opened in browser")
} else {
  console.log("Failed to open URL")
}
```

**Notes:**
- Implemented via `openUrl` command
- Uses platform-specific handlers (`open` on macOS, `xdg-open` on Linux)
- Supports `http://`, `https://`, `mailto:`, `file://` schemes

---

### coconut.notify()

**Signature:** `await coconut.notify(title: string, body: string): Promise<boolean>`

**Description:** Show a native system notification.

**Example:**

```js
const ok = await coconut.call("notify", { 
  title: "Build Complete", 
  body: "Your app compiled successfully" 
})
if (ok) {
  console.log("Notification shown")
}
```

**Notes:**
- Implemented via `notify` command
- Platform-specific implementation (NSUserNotification on macOS)
- May require user permission on some platforms

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

- See **[Examples](../examples/examples.md)** for real-world usage patterns
- Read **[Troubleshooting](../explanation/troubleshooting.md)** for common errors and solutions
