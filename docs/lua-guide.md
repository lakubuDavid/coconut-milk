# Lua Backend Guide

Coconut Milk uses **Lua** (via LuaJIT) as the application authoring language. All backend logic — commands, configuration, event handling — lives in Lua files.

---

## Commands

Commands are the primary way the frontend interacts with Lua. Each command is a function that receives a payload table and the runtime context.

### Inline Commands

Register commands directly in `main.lua`:

```lua
function coconut.commands(ctx)
  ctx:bind("ping", function(params, ctx)
    return { message = "pong" }
  end)

  ctx:bind("greet", function(params, ctx)
    local name = params.name or "World"
    return { greeting = "Hello, " .. name .. "!" }
  end)
end
```

### Command Files

For larger projects, organize commands in the `commands/` folder:

```
my-app/
├── commands/
│   ├── greet.lua       # Greeting commands
│   ├── filesystem.lua  # File operations
│   └── settings.lua    # App settings
```

Each file returns a table of command functions:

```lua
-- commands/greet.lua
---@command greet
---@param params { name?: string }
---@return { greeting: string }
local function greet(params, ctx)
  local name = params.name or "World"
  return { greeting = "Hello, " .. name .. "!" }
end

---@command farewell
---@param params { name?: string }
---@return { message: string }
local function farewell(params, ctx)
  return { message = "Goodbye, " .. (params.name or "friend") }
end

return {
  greet = greet,
  farewell = farewell,
}
```

### @command Annotations

The `---@command` annotation enables **code generation**:

```lua
---@command greet                  -- Required: command name
---@param params { name?: string } -- Optional: parameter types
---@return { greeting: string }    -- Optional: return type
local function greet(params, ctx)
  -- ...
end
```

Running `coconut generate` scans all `commands/*.lua` files, extracts these annotations, and generates:

- `generated/greet.g.lua` — Lua registration glue
- `generated/greet.d.ts` — TypeScript type definitions
- `generated/greet.g.js` — JavaScript wrapper functions
- `generated/commands.d.ts` — Aggregated command name union type

### Command Handler Signature

Every command handler receives two arguments:

```lua
---@alias CoconutCommandFn fun(params: table, ctx: CoconutContext): any
```

| Argument | Type | Description |
|---|---|---|
| `params` | `table` | The payload from `coconut.call(name, payload)`. Always a table — never `nil`. |
| `ctx` | `CoconutContext` | The runtime context. Provides `emit`, `show`, `reload`, `close`, etc. |

### Return Values

Commands can return:

| Return Type | Example | Frontend Receives |
|---|---|---|
| **Table** | `return { ok = true }` | JS object: `{ ok: true }` |
| **String** | `return "Hello!"` | JS string: `"Hello!"` |
| **Number** | `return 42` | JS number: `42` |
| **Boolean** | `return true` | JS boolean: `true` |
| **Nil** | `return nil` | JS `undefined` |
| **Nothing** | (no return) | JS `undefined` |

**Array serialization:** Sequential 1-indexed Lua tables become JSON arrays:

```lua
return { "a", "b", "c" }
-- Frontend receives: ["a", "b", "c"]
```

### Error Handling in Commands

Use `pcall` for safety around native calls:

```lua
local function read_file(params, ctx)
  local ok, result = pcall(coconut.fs.readText, params.path)
  if not ok then
    return { error = "Failed to read file: " .. tostring(result) }
  end
  return { content = result }
end
```

Or return error tables:

```lua
local function divide(params, ctx)
  if params.b == 0 then
    return { error = "division by zero" }
  end
  return { result = params.a / params.b }
end
```

### Command Registration Order

Commands are auto-loaded from `commands/` at startup. The loading order is:

1. `coconut.config.lua` is parsed (file-based config)
2. `coconut.config(ctx)` is called (runtime config)
3. `commands/` folder is scanned and loaded
4. `coconut.commands(ctx)` is called (manual bindings)

**Important:** Manual `ctx:bind()` calls in `coconut.commands()` happen **after** auto-loaded commands. If you bind a command with the same name as an auto-loaded one, it will **fail** with a `DuplicateCommand` error.

---

## Config Callback

The `coconut.config(ctx)` callback is the **required** entry point for configuring your app.

### Basic Configuration

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

### Window Settings

| Method | Signature | Description |
|---|---|---|
| `setWindowSize` | `({w, h}) → ctx` | Initial window size |
| `setMinimumWindowSize` | `({w, h}) → ctx` | Minimum resize size |
| `setMaximumWindowSize` | `({w, h}) → ctx` | Maximum resize size |
| `setMinimumWindowWidth` | `(w) → ctx` | Minimum width only |
| `setMinimumWindowHeight` | `(h) → ctx` | Minimum height only |
| `setMaximumWindowWidth` | `(w) → ctx` | Maximum width only |
| `setMaximumWindowHeight` | `(h) → ctx` | Maximum height only |
| `setTitle` | `(string) → ctx` | Window title |
| `setResizable` | `(bool) → ctx` | Allow resizing |
| `setFrameless` | `(bool) → ctx` | Remove window chrome (macOS) |
| `setTransparent` | `(bool) → ctx` | Transparent background (macOS) |
| `setBackgroundColor` | `(r, g, b, a) → ctx` | RGBA background (0.0-1.0) |
| `setInitialView` | `(string) → ctx` | View to show at startup |

### Frameless Windows

```lua
function coconut.config(ctx)
  return ctx
    :setFrameless(true)
    :setTransparent(true)
    :setBackgroundColor(0, 0, 0, 0)  -- Fully transparent
    :setWindowSize({ w = 480, h = 700 })
end
```

When `transparent` is true, the frontend receives a `transparent-window` CSS class on `<body>`:

```css
body.transparent-window {
  background: transparent;
  /* Custom styling for transparent window */
}
```

### Chainable Methods

All config methods return `ctx`, enabling fluent chaining:

```lua
function coconut.config(ctx)
  return ctx
    :setWindowSize({ w = 1280, h = 640 })
    :setTitle("My App")
    :setInitialView("home")
    :setResizable(true)
end
```

---

## Views

The `coconut.views()` function returns a table of named view descriptors.

### View Factories

```lua
function coconut.views()
  return {
    -- Load from file (relative to app root)
    home = View.load("views/home.html"),

    -- Inline HTML
    about = View.html("<h1>About</h1>"),

    -- External URL
    docs = View.url("https://example.com/docs"),
  }
end
```

### View Resolution

- `View.load("views/home.html")` → resolves to `./views/home.html` relative to the app root
- `View.html("<h1>Hello</h1>")` → served via temp file + `file://` URL
- `View.url("https://...")` → navigated directly in the webview

### Lifecycle Callbacks

Views support four lifecycle methods:

```lua
local home_view = View.load("views/home.html")
  :on_load(function(ctx)
    -- Called once when the view is first created
    -- Good for: initializing data, loading config
    print("Home loaded")
  end)
  :on_mount(function(ctx)
    -- Called every time the view becomes visible
    -- Good for: focusing input, starting timers
    print("Home mounted")
  end)
  :on_unmount(function(ctx)
    -- Called every time the view is hidden
    -- Good for: stopping timers, saving state
    print("Home unmounted")
  end)
  :on_frontend_event("navigate", function(name, payload, ctx)
    -- Called when frontend emits this event while view is active
    -- Good for: in-view navigation, toasts
    if payload.view then
      ctx:show(payload.view)
    end
  end)
```

### View Switching

```lua
-- Switch to a view by name:
ctx:show("settings")

-- Reload the current view:
ctx:reload()
```

### View Props (Spec, Not Fully Implemented)

The spec defines `ctx:show(name, props)` for passing props to views, but this is not fully implemented in v0.1.

---

## Events

### Lua → Frontend

```lua
-- Emit an event to the frontend
ctx:emit("toast", { message = "Saved successfully!", type = "success" })

-- Synchronous emit (blocks until delivered)
ctx:emit_sync("critical", { reason = "data loss risk" })
```

### Frontend → Lua (Global Dispatcher)

```lua
-- Global event dispatcher
function coconut.events(name, payload, ctx)
  if name == "navigate" then
    ctx:show(payload.view)
  elseif name == "save_request" then
    -- Handle save request from frontend
    local ok = save_data(payload.data)
    ctx:emit("save_response", { ok = ok })
  end
end
```

### Lifecycle Hooks

| Hook | Signature | When Called |
|---|---|---|
| `coconut.on_resize` | `(ctx, w, h)` | Window is resized |

```lua
function coconut.on_resize(ctx, w, h)
  -- Notify frontend about window resize
  ctx:emit("window_resized", { w = w, h = h })
end
```

### View-Specific Events

Events attached to views via `on_frontend_event` only fire when that view is active:

```lua
View.load("views/home.html")
  :on_frontend_event("toast", function(name, payload, ctx)
    -- Only fires when "home" view is active
    ctx:emit("toast_shown", payload)
  end)
```

---

## Lua HTML DSL

Coconut Milk works with any **pure Lua HTML DSL** for generating HTML without build tools. The library is an external Lua file that uses metatables for dynamic tag generation.

For a minimal 117-line example see the [lua-html-app](../examples/lua-html-app/) example. For a curated list of Lua template engines and web libraries see [awesome-lua](https://github.com/LewisJEllis/awesome-lua#templating) — notable options include [lustache](http://olivinelabs.com/lustache/) (Mustache), [etlua](https://github.com/leafo/etlua) (ERB-style), and [lua-resty-template](https://github.com/bungle/lua-resty-template) (Jinja-like).

### Usage

Drop `html.lua` into your project's `lib/` folder:

```lua
-- lib/html.lua (vendored from https://riki.house/lua-html)
-- 117-line pure Lua HTML DSL
```

Then use it in your views:

```lua
-- main.lua
local html = require("lib.html")

function coconut.views()
  return {
    home = View.html(tostring(html.div({ class = "app" },
      html.h1({}, "Hello, World!"),
      html.p({}, "This is generated by Lua."),
      html.button({ onclick = "alert('clicked')" }, "Click Me")
    ))),
  }
end
```

### How It Works

The DSL uses metatables to intercept undefined keys:

```lua
local html = {}
setmetatable(html, {
  __index = function(_, tag)
    return function(attrs, ...)
      -- Returns an Html object that renders to <tag>...</tag>
    end
  end,
})
```

Calling `html.div(...)` creates an `Html` object. Calling `tostring()` on it renders the HTML string. This works with `View.html()` because `View.html()` accepts a string.

### Example

```lua
local html = require("lib.html")

local page = html.html({},
  html.head({},
    html.meta({ charset = "UTF-8" }),
    html.meta({ name = "viewport", content = "width=device-width, initial-scale=1" }),
    html.title({}, "My App"),
    html.link({ rel = "stylesheet", href = "coconut://assets/style.css" })
  ),
  html.body({},
    html.div({ id = "app" },
      html.h1({}, "Hello from Lua!"),
      html.p({}, "No build step needed.")
    ),
    html.script({ src = "coconut://assets/app.js" })
  )
)

function coconut.views()
  return {
    app = View.html(tostring(page)),
  }
end
```

### Limitations

- **No component system** — Plain HTML generation, no reactivity
- **No templating** — Manual string concatenation for dynamic content
- **No build step** — Generated at startup, not at runtime
- **External dependency** — Template libraries are vendored, not part of Coconut Milk core

---

## Third-Party Lua Libraries

Lua has a rich ecosystem of third-party libraries maintained by the community. The canonical curated list is **[awesome-lua](https://github.com/LewisJEllis/awesome-lua)** on GitHub. Categories particularly relevant for Coconut Milk backend work:

| Category | Notable libraries | Notes |
|---|---|---|
| **Templating** | [lustache](http://olivinelabs.com/lustache/), [etlua](https://github.com/leafo/etlua), [lua-resty-template](https://github.com/bungle/lua-resty-template) | HTML generation, ERB/Jinja-style syntax |
| **JSON** | [lua-cjson](https://github.com/mpx/lua-cjson/), [json.lua](https://github.com/rxi/json.lua), [dkjson](http://dkolf.de/src/dkjson-lua.fsl/home) | Fast C-based or pure Lua encoding/decoding |
| **HTTP / Networking** | [LuaSocket](https://github.com/diegonehab/luasocket), [lua-http](https://github.com/daurnimator/lua-http), [lua-cURL](https://github.com/Lua-cURL/Lua-cURLv3) | HTTP clients, servers, WebSockets |
| **File system** | [LuaFileSystem](http://keplerproject.github.io/luafilesystem/) | POSIX file system access |
| **CLI / Args** | [argparse](https://github.com/mpeterv/argparse), [cliargs](https://github.com/amireh/lua_cliargs) | Command-line argument parsing |
| **Testing** | [busted](http://olivinelabs.com/busted/) | BDD-style unit testing framework |
| **Logging** | [lua-log](https://github.com/moteus/lua-log), [LuaLogging](https://github.com/Neopallium/lualogging) | Async/sync loggers with multiple appenders |
| **Serialization** | [serpent](https://github.com/pkulchenko/serpent), [Ser](https://github.com/gvx/Ser) | Pretty-printing and serializing Lua tables |
| **Data stores** | [LuaSQL](http://keplerproject.github.io/luasql/), [pgmoon](https://github.com/leafo/pgmoon) | Database drivers (SQLite, PostgreSQL, MySQL…) |

For the full list (1,500+ entries) see [awesome-lua](https://github.com/LewisJEllis/awesome-lua).

---

## Patterns & Best Practices

### Project Organization

```
my-app/
├── main.lua                # Entry point (config + views)
├── coconut.config.lua      # Optional defaults
├── commands/               # Backend logic
│   ├── greet.lua           # Greeting commands
│   ├── filesystem.lua      # File operations
│   └── settings.lua        # Settings management
├── views/                  # Frontend views
│   ├── home.html           # Main view
│   ├── settings.html       # Settings view
│   └── style.css           # Shared styles
├── assets/                 # Static assets
│   ├── app.js              # Frontend logic
│   └── icon.png            # Images
└── lib/                    # Vendored libraries
    └── html.lua            # Lua HTML DSL
```

### Error Handling Pattern

```lua
local function safe_operation(params, ctx)
  local ok, result = pcall(function()
    -- Potentially failing operation
    return coconut.fs.readText(params.path)
  end)

  if not ok then
    -- Return error table instead of throwing
    return { error = "Operation failed: " .. tostring(result) }
  end

  return { data = result }
end
```

### State Management Pattern

```lua
-- Module-level state (persists across command calls)
local settings = {
  theme = "dark",
  precision = 2,
  sound = true,
}

local function save_settings(params, ctx)
  if params.theme then settings.theme = params.theme end
  if params.precision then settings.precision = params.precision end
  if params.sound ~= nil then settings.sound = params.sound end
  return { ok = true }
end

local function load_settings(params, ctx)
  return settings
end

return {
  save_settings = save_settings,
  load_settings = load_settings,
}
```

### Event-Driven Pattern

```lua
-- Command emits event, frontend responds
local function save_file(params, ctx)
  local ok = write_to_disk(params.path, params.content)
  ctx:emit("file_saved", { path = params.path, ok = ok })
  return { ok = ok }
end

-- Frontend listens and shows toast
coconut.on("file_saved", (payload) => {
  if (payload.ok) {
    showToast(`Saved: ${payload.path}`)
  } else {
    showToast(`Failed to save: ${payload.path}`, "error")
  }
})
```

---

## Limitations

### Single Window

Coconut Milk is **single-window only**. You cannot create multiple windows. For multi-page apps, use view switching (`ctx:show()`) with named views.

### No Threading Model

All Lua commands run **synchronously** on the main thread. There is no built-in async/await or worker thread support. Long-running commands will block the UI.

**Workaround:** For long operations (file processing, network requests), use the filesystem directly and emit progress events:

```lua
local function process_large_file(params, ctx)
  local lines = coconut.fs.readText(params.path):split("\n")
  for i, line in ipairs(lines) do
    if i % 100 == 0 then
      ctx:emit("progress", { current = i, total = #lines })
    end
    -- Process line...
  end
  return { ok = true }
end
```

### Table-Only Payloads

All bridge payloads must be **Lua tables** (or JS objects). Raw primitives (strings, numbers, booleans) are not supported as payloads:

```lua
-- ✅ Correct:
ctx:emit("event", { message = "hello" })

-- ❌ Wrong:
ctx:emit("event", "hello")
```

### No Binary Data in Bridge

Binary data (images, files) **cannot** be sent through the JSON bridge. Use file paths instead:

```lua
-- ✅ Correct: Return file path, frontend loads via <img src="file://...">
return { type = "image", path = "/abs/path/to/image.png" }

-- ❌ Wrong: Send binary data through JSON
return { type = "image", data = <binary bytes> }
```

### Limited Platform Features

| Feature | macOS | Windows | Linux |
|---|---|---|---|
| Frameless window | ✅ | 🔲 | 🔲 |
| Transparent background | ✅ | 🔲 | 🔲 |
| coconut:// scheme | ✅ | 🔲 | 🔲 |
| Native dialogs | ✅ | ✅ | ✅ |
| Filesystem | ✅ | ✅ | ✅ |

---

## Next Steps

- Read the **[Bridge (Advanced)](./bridge.md)** for protocol details
- Check the **[API Reference](./api-reference.md#lua-api)** for all Lua functions
- See **[Examples](./examples.md)** for real-world patterns
