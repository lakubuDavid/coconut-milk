# Coconut Milk — Quick Reference

Concise reference tables. For full documentation, see `SKILL.md` and `skill-guide.md`.

---

## Startup Lifecycle

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

---

## Log Levels

| Config value | Shows |
|---|---|
| `"debug"` | All messages |
| `"info"` | `[INFO]`, `[WARN]`, `[ERROR]` (default) |
| `"warn"` | `[WARN]`, `[ERROR]` |
| `"error"` | Only `[ERROR]` |

---

## Config File Format (`coconut.config.lua`)

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

---

## Command Error Codes (JS)

| Code | Meaning |
|---|---|
| `CommandNotFound` | No handler registered for this name |
| `LuaError` | Lua handler threw an error |
| `NotReady` | Bridge not ready yet |
| `QueueOverflow` | Event queue filled before bridge ready |
| `InvalidPayload` | Payload wasn't a table |
| `BridgeError` | Transport-level failure |

---

## View Factories

| Factory | Example |
|---|---|
| `View.load("path.html")` | Local file relative to project root |
| `View.html("<h1>Hi</h1>")` | Inline HTML (written to temp file) |
| `View.url("https://...")` | External URL |

---

## Filesystem API (Lua)

```lua
coconut.fs.readText(path)             -- -> string or nil
coconut.fs.writeText(path, content)   -- -> boolean
coconut.fs.exists(path)               -- -> boolean
coconut.fs.listDir(path)              -- -> array of {name, path, is_dir}
```

---

## Dialog API (Lua)

```lua
-- Always wrap in pcall:
local ok, result = pcall(coconut.dialog.open, "Title", multi_select, choose_dir)
-- result = { confirmed, path, is_dir, paths }

local ok, result = pcall(coconut.dialog.save, "Title", "default.txt")
-- result = { confirmed, path }

local ok, result = pcall(coconut.dialog.messageBox, "Title", "Message", "info")
-- type: "info" | "warning" | "error" | "question"
```

---

## Window Controls (JS)

```js
await coconut.window.minimize()
await coconut.window.toggleFullscreen()
await coconut.window.close()
```

---

## Bridge Flow

```
Frontend                          Runtime (C++)                    Lua
   |                                   |                            |
   |-- coconut.call(name, payload) --> |                            |
   |   (RPC: type=call, id=uuid)       |                            |
   |                                   |-- dispatch(name, payload) ->|
   |                                   |                            |
   |                                   |<--------- result ----------|
   |                                   |   (return value serialised) |
   |<------ Promise resolves -----------|                            |
   |   { ok: true, data: ... }         |                            |

   |                                   |                            |
   |<-- ctx:emit(name, payload) ------ |   (Lua -> JS via queue)    |
   |   (coconut.on listener fires)     |                            |
```

---

## coconut:// Scheme

| URL | Resolves to |
|---|---|
| `coconut://assets/style.css` | `{project_root}/assets/style.css` |
| `coconut://views/app.js` | `{project_root}/views/app.js` |
| `coconut://settings` | View named `"settings"` → `ctx:show("settings")` |

Platform: **macOS only** (WKWebView scheme handler).