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
  :on("navigate", function(event)
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
ctx:emit({ name = "toast", message = "Saved successfully!", type = "success" })

-- Synchronous emit (blocks until delivered)
ctx:emit_sync({ name = "critical", reason = "data loss risk" })
```

### Frontend → Lua (Global Dispatcher)

```lua
-- Global event dispatcher (last resort fallback)
function coconut.events(event)
  if event.name == "navigate" then
    ctx:show(event.view)
  elseif event.name == "save_request" then
    -- Handle save request from frontend
    local ok = save_data(event.data)
    ctx:emit({ name = "save_response", ok = ok })
  end
end
```

### Lifecycle Events

Lifecycle events use the same `coconut.on()` API as any other event:

| Event | Payload fields | When fired |
|---|---|---|
| `resize` | `w`, `h` | Window resized |
| `focus` | — | Window gained focus |
| `blur` | — | Window lost focus |
| `ready` | — | App initialized, before event loop |
| `close` | — | App closing (cancellable) |

```lua
coconut.on("resize", function(event)
  -- Notify frontend about window resize
  ctx:emit({ name = "window_resized", w = event.w, h = event.h })
end)
```

### View-Specific Events

Events attached to views via `view:on()` only fire when that view is active,
and fire before global `coconut.on()` callbacks:

```lua
View.load("views/home.html")
  :on("toast", function(event)
    -- Only fires when "home" view is active
    ctx:emit({ name = "toast_shown", message = event.message })
  end)
```

---

## Built-in Modules

Coconut Milk provides several built-in modules for common tasks. These are available globally in your Lua scripts.

### Keybinds (`coconut.keybind`)

Register keyboard shortcuts that work across your application.

**Basic usage:**

```lua
-- Register a simple keybind
coconut.keybind("CmdOrCtrl+S", function()
  print("Save triggered")
  save_current_document()
end)

-- With options
coconut.keybind("CmdOrCtrl+Shift+P", function()
  open_command_palette()
end, {
  description = "Open command palette",
  global = true  -- Works even when app is not focused
})
```

**Key format:**
- Use `CmdOrCtrl` for cross-platform modifier (Cmd on macOS, Ctrl on Windows/Linux)
- Combine modifiers: `CmdOrCtrl+Shift+Alt+K`
- Named keys: `Enter`, `Escape`, `Space`, `Tab`, `Up`, `Down`, `Left`, `Right`
- Function keys: `F1` through `F12`

**Unregistering keybinds:**

```lua
local unregister = coconut.keybind("CmdOrCtrl+K", handler)
-- Later...
unregister()  -- Remove the keybind
```

**Scoping keybinds to views:**

```lua
function coconut.views()
  return {
    editor = View.load("views/editor.html")
      :on_mount(function()
        -- Only active when editor view is shown
        coconut.keybind("CmdOrCtrl+S", save_editor_content, {
          scope = "editor"
        })
      end)
  }
end
```

---

### CLI Arguments (`coconut.args`)

Access command-line arguments passed to your application.

**Structure:**

```lua
coconut.args = {
  positional = {"arg1", "arg2"},  -- Positional arguments
  named = {mode = "dev", port = "3000"},  -- Named arguments (--key=value)
  flags = {verbose = true, debug = false}  -- Boolean flags (--flag)
}
```

**Example usage:**

```lua
-- Check for verbose flag
if coconut.args.flags.verbose then
  print("Verbose mode enabled")
end

-- Get named argument
local port = coconut.args.named.port or "8080"
print("Starting server on port " .. port)

-- Process positional arguments
for i, file in ipairs(coconut.args.positional) do
  print("Processing file: " .. file)
  process_file(file)
end
```

**Command-line examples:**

```bash
# Positional arguments
coconut file1.txt file2.txt
# coconut.args.positional = {"file1.txt", "file2.txt"}

# Named arguments
coconut --port=3000 --mode=production
# coconut.args.named = {port = "3000", mode = "production"}

# Flags
coconut --verbose --debug
# coconut.args.flags = {verbose = true, debug = true}

# Mixed
coconut --verbose file.txt --output=result.json
# coconut.args.positional = {"file.txt"}
# coconut.args.named = {output = "result.json"}
# coconut.args.flags = {verbose = true}
```

---

### Environment Variables (`coconut.env`)

Access system environment variables with lazy loading.

**Basic usage:**

```lua
-- Access environment variables
local home = coconut.env.HOME  -- or USERPROFILE on Windows
local path = coconut.env.PATH
local node_env = coconut.env.NODE_ENV or "development"

print("Home directory: " .. home)
print("Environment: " .. node_env)
```

**Special keys:**

```lua
-- Current working directory
local cwd = coconut.env.cwd
print("Working in: " .. cwd)

-- User's home directory (cross-platform)
local homedir = coconut.env.homedir
print("Home: " .. homedir)

-- Platform path separator (/ on Unix, \ on Windows)
local sep = coconut.env.pathSeparator
local config_path = homedir .. sep .. ".config" .. sep .. "myapp"
```

**Lazy loading:**

`coconut.env` uses a metatable with `__index` to load environment variables on-demand. This means:
- Variables are only read when accessed
- Missing variables return `nil` (no error)
- No performance penalty for unused variables

---

### JSON Utilities (`coconut.json`)

Convert between Lua tables and JSON strings.

**Serialize to JSON:**

```lua
local data = {
  name = "Alice",
  age = 30,
  hobbies = {"reading", "coding"}
}

local json_string = coconut.json.stringify(data)
print(json_string)
-- Output: {"name":"Alice","age":30,"hobbies":["reading","coding"]}
```

**Parse JSON:**

```lua
local json_string = '{"name":"Bob","active":true}'
local data = coconut.json.parse(json_string)

print(data.name)    -- "Bob"
print(data.active)  -- true
```

**Error handling:**

```lua
-- parse() returns nil on invalid JSON
local result = coconut.json.parse("not valid json")
if result == nil then
  print("Failed to parse JSON")
end

-- Use pcall for more control
local ok, parsed = pcall(coconut.json.parse, json_string)
if not ok then
  print("JSON error: " .. tostring(parsed))
end
```

**Pretty printing:**

```lua
local data = {status = "ok", items = {1, 2, 3}}
local pretty = coconut.json.stringify(data, {pretty = true})
print(pretty)
-- Output:
-- {
--   "status": "ok",
--   "items": [
--     1,
--     2,
--     3
--   ]
-- }
```

---

### Clipboard (`coconut.clipboard`)

Read from and write to the system clipboard.

**Write text:**

```lua
coconut.clipboard.writeText("Hello, clipboard!")
print("Text copied to clipboard")
```

**Read text:**

```lua
local text = coconut.clipboard.readText()
if text then
  print("Clipboard contains: " .. text)
else
  print("Clipboard is empty or unavailable")
end
```

**Practical example:**

```lua
-- Copy command result to clipboard
coconut.keybind("CmdOrCtrl+Shift+C", function()
  local result = calculate_result()
  coconut.clipboard.writeText(tostring(result))
  ctx:emit({ name = "toast", message = "Result copied to clipboard" })
end)

-- Paste from clipboard
coconut.keybind("CmdOrCtrl+V", function()
  local text = coconut.clipboard.readText()
  if text then
    insert_text_at_cursor(text)
  end
end)
```

**Notes:**
- Only supports plain text (not images or rich text)
- Returns `nil` if clipboard is empty or access is denied
- Works cross-platform (macOS, Windows, Linux)

---

### Open URL (`coconut.openUrl`)

Open URLs in the system's default browser or application.

**Basic usage:**

```lua
-- Open a website
coconut.openUrl("https://example.com")

-- Open a local file
coconut.openUrl("file:///path/to/document.pdf")

-- Open mailto link
coconut.openUrl("mailto:user@example.com?subject=Hello")
```

**Error handling:**

```lua
local ok, err = pcall(coconut.openUrl, "https://example.com")
if not ok then
  print("Failed to open URL: " .. tostring(err))
end
```

**Practical example:**

```lua
-- Open documentation in browser
coconut.keybind("F1", function()
  coconut.openUrl("https://docs.example.com")
end, { description = "Open documentation" })

-- Open file in default application
function open_file(path)
  local url = "file://" .. path
  coconut.openUrl(url)
end
```

**Supported URL schemes:**
- `http://` and `https://` — Web browsers
- `file://` — Default application for file type
- `mailto:` — Email client
- `tel:` — Phone application (if available)
- Custom schemes registered on the system

---

### Notifications (`coconut.notify`)

Display system notifications to the user.

**Basic usage:**

```lua
coconut.notify("Hello!", "This is a notification message")
```

**With options:**

```lua
coconut.notify("Build Complete", "Your project has been built successfully", {
  icon = "success",  -- "info", "warning", "error", "success"
  sound = true       -- Play notification sound (if enabled)
})
```

**Practical examples:**

```lua
-- Notify on save
function save_document()
  local ok = write_to_file(current_file, content)
  if ok then
    coconut.notify("Saved", "Document saved successfully", { icon = "success" })
  else
    coconut.notify("Error", "Failed to save document", { icon = "error" })
  end
end

-- Notify on background task completion
function process_files(files)
  ctx:emit({ name = "processing_started" })
  
  -- Simulate background work
  for i, file in ipairs(files) do
    process_file(file)
  end
  
  coconut.notify(
    "Processing Complete",
    string.format("Processed %d files", #files),
    { icon = "success" }
  )
end
```

**Platform behavior:**
- **macOS:** Uses native NSUserNotificationCenter
- **Windows:** Uses toast notifications (Windows 10+) or balloon notifications
- **Linux:** Uses libnotify (requires notification daemon)

**Notes:**
- Notifications are non-blocking (return immediately)
- User may have notifications disabled at system level
- Keep messages short and informative
- Use appropriate icon types for context

---

### Logging (`coconut.log`, `coconut.info`, `coconut.warn`, `coconut.error`)

**Signature:** `coconut.log(msg: string) → nil` (and similarly for info, warn, error)

Print messages to the debug log with severity level filtering.

**Example:**

```lua
coconut.log("starting app")
coconut.info("verbode mode enabled")
coconut.warn("deprecated API used")
coconut.error("connection failed")
```

**Notes:**
- Output goes to stdout/stderr depending on level
- Visible when running with `--debug` flag
- Unlike Lua's `print()`, these respect log level filtering

---

### Key-Value Store (`coconut.store`)

**Signature:** `coconut.store` (table)

In-memory key-value store shared between Lua and JavaScript.

**Methods:**

| Method | Signature | Description |
|---|---|---|
| `set` | `coconut.store.set(key, value)` | Set a key-value pair |
| `get` | `coconut.store.get(key)` → `value or nil, err` | Get value by key |
| `has` | `coconut.store.has(key)` → `boolean` | Check if key exists |
| `delete` | `coconut.store.delete(key)` | Remove a key-value pair |
| `clear` | `coconut.store.clear()` | Remove all entries |
| `keys` | `coconut.store.keys()` → `table` | Return all keys |

**Example:**

```lua
coconut.store.set("username", "ada")
local name = coconut.store.get("username")  -- "ada"
local exists = coconut.store.has("username") -- true
for _, k in ipairs(coconut.store.keys()) do
  print(k)
end
coconut.store.delete("username")
```

**Notes:**
- In-memory only, not persisted to disk
- Changes are visible to both Lua and JavaScript
- `get` returns `nil, error_message` for missing keys
- Thread-safe for concurrent access

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
  ctx:emit({ name = "file_saved", path = params.path, ok = ok })
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
      ctx:emit({ name = "progress", current = i, total = #lines })
    end
    -- Process line...
  end
  return { ok = true }
end
```

### Table-Only Payloads

All event payloads must be **Lua tables** with a `name` field.
Payload fields are merged directly into the event object:

```lua
-- ✅ Correct:
ctx:emit({ name = "event", message = "hello" })

-- ❌ Wrong (no name field):
ctx:emit({ message = "hello" })
```

### Dispatch Chain Order

When an event is emitted, it goes through three tiers:
1. **View scope** — `view:on(name, fn)` fires if the active view has a listener
2. **Global subscribe** — `coconut.on(name, fn)` listeners fire in registration order
3. **Fallback** — `coconut.events(event)` fires last, if reached

Call `event:stopPropagation()` to prevent reaching the next tier.
Call `event:preventDefault()` on cancellable events to skip the default action.

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
- See **[Examples](../examples/examples.md)** for real-world patterns
