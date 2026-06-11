# Coconut Milk API Reference

## Lua API

### `coconut.config(ctx)`

Called at startup to configure the application. Returns `ctx` (or a table that will be merged into the config).

```lua
function coconut.config(ctx)
  return ctx
    :setWindowSize({ w = 1280, h = 640 })
    :setInitialView("home")
    :setTitle("My App")
end
```

### Context methods

| Method | Signature | Description |
|---|---|---|
| `setWindowSize` | `({w: number, h: number}) → ctx` | Initial window size |
| `setMinimumWindowSize` | `({w: number, h: number}) → ctx` | Minimum window size |
| `setMaximumWindowSize` | `({w: number, h: number}) → ctx` | Maximum window size |
| `setMinimumWindowWidth` | `(w: number) → ctx` | Minimum window width |
| `setMinimumWindowHeight` | `(h: number) → ctx` | Minimum window height |
| `setMaximumWindowWidth` | `(w: number) → ctx` | Maximum window width |
| `setMaximumWindowHeight` | `(h: number) → ctx` | Maximum window height |
| `setTitle` | `(title: string) → ctx` | Window title |
| `setResizable` | `(resizable: boolean) → ctx` | Window resizable flag |
| `setFrameless` | `(frameless: boolean) → ctx` | Frameless window |
| `setTransparent` | `(transparent: boolean) → ctx` | Transparent background |
| `setInitialView` | `(name: string) → ctx` | Initial view name |
| `setBackgroundColor` | `(r, g, b, a) → ctx` | Window background color |
| `bind` | `(name: string, fn) → ctx` | Register a command |
| `emit` | `(name: string, payload: table)` | Emit event to frontend |
| `emit_sync` | `(name: string, payload: table)` | Emit event synchronously |
| `show` | `(name: string)` | Switch to view |
| `reload` | `()` | Reload current view |
| `close` | `()` | Close window |

### `coconut.views()`

Returns a table of named view descriptors.

```lua
function coconut.views()
  return {
    home = View.load("views/home.html"),
    about = View.html("<h1>About</h1>"),
    external = View.url("https://example.com"),
  }
end
```

### View factories

| Factory | Description |
|---|---|
| `View.load(path)` | Load from a local HTML file |
| `View.html(html)` | Inline HTML string |
| `View.url(url)` | External URL |

### View descriptor callbacks

```lua
View.load("views/home.html")
  :on_load(function(ctx)
    -- Called once when the view is first created
  end)
  :on_mount(function(ctx)
    -- Called every time the view becomes visible
  end)
  :on_unmount(function(ctx)
    -- Called every time the view is hidden
  end)
  :on_frontend_event("navigate", function(name, payload, ctx)
    -- Called when frontend emits this event while view is active
  end)
```

### `coconut.commands(ctx)`

Register commands manually (alternative to auto-loading from `commands/`).

```lua
function coconut.commands(ctx)
  ctx:bind("hello", function(params, ctx)
    return "Hello, " .. (params.name or "user")
  end)
end
```

### `coconut.events(name, payload, ctx)`

Global frontend event dispatcher.

```lua
function coconut.events(name, payload, ctx)
  if name == "navigate" then
    ctx:show(payload.view)
  end
end
```

---

## Frontend API (`coconut` global)

### `coconut.ready()`

Returns a `Promise<void>` that resolves when the bridge is ready.

```js
await coconut.ready()
```

### `coconut.call(name, payload)`

Call a Lua command. Returns a `Promise<T>`.

```js
const result = await coconut.call("hello", { name: "Ada" })
```

### `coconut.emit(name, payload)`

Emit an event to Lua. Returns a `Promise<void>`.

```js
await coconut.emit("frontend_ready", { at: Date.now() })
```

### `coconut.on(name, callback)`

Listen for Lua-emitted events. Returns an unsubscribe function.

```js
const unsub = coconut.on("toast", (payload) => {
  console.log(payload.message)
})

// Later:
unsub()
```

### `coconut.views()`

Returns a `Promise<string[]>` with the list of registered view names.

```js
const names = await coconut.views()
```

### `coconut.ping()`

Returns a `Promise<string>` for connectivity testing.

```js
const pong = await coconut.ping()
```

### `coconut.window` helpers

```js
await coconut.window.minimize()
await coconut.window.toggleFullscreen()
await coconut.window.close()
```

### `coconut.fs` helpers

```js
const { ok, data, error } = await coconut.fs.readText("/path/to/file")
```

---

## Config file (`coconut.config.lua`)

```lua
return {
  window_width = 1280,
  window_height = 640,
  window_min_width = 800,
  window_min_height = 600,
  window_max_width = 1920,
  window_max_height = 1080,
  initial_view = "home",
  title = "My App",
  frameless = false,
  transparent = false,
  resizable = true,
  view_root = "views",
  asset_root = "assets",
  command_root = "commands",
  generators = {
    output_dir = "generated"
  },
  views = {
    home = { kind = "file", src = "views/home.html" },
  }
}
```

---

## Error codes

| Code | Description |
|---|---|
| `Ok` | Success |
| `Unknown` | Unknown error |
| `InvalidConfig` | Config file parse error |
| `InvalidView` | Invalid view descriptor |
| `MissingFile` | File not found |
| `DuplicateCommand` | Command already registered |
| `CommandNotFound` | Command not found |
| `InvalidPayload` | Invalid payload format |
| `NotReady` | Bridge not ready |
| `QueueOverflow` | Event queue overflow |
| `LuaError` | Lua runtime error |
| `BridgeError` | Bridge protocol error |
| `WebViewError` | Webview error |
| `ParseError` | Parse error |

---

## Platform support

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
| View callbacks | ✅ | ✅ | ✅ |

---

## Bridge protocol

Messages are JSON envelopes with the following shape:

```json
{ "type": "call",   "id": "uuid", "name": "cmd", "payload": {} }
{ "type": "return", "id": "uuid", "payload": <any> }
{ "type": "error",  "id": "uuid", "payload": { "code": "...", "message": "..." } }
{ "type": "event",  "name": "evt", "payload": {} }
{ "type": "ready" }
```

- `id` is required for `call`, `return`, and `error`
- `name` is required for `call` and `event`
- `payload` is a JSON value (object, array, or primitive)
- `ready` has no `id` and carries no payload
