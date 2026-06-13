# Examples

Real-world example projects included with Coconut Milk, plus patterns for common use cases.

---

## Example Projects

### calculator-vue

A multi-page Vue.js calculator app with settings persistence.

**Location:** `examples/calculator-vue/`

**Tech stack:**
- Vue.js 3 (SFC)
- Vite (build tool)
- localStorage (settings persistence)

**Features:**
- Calculator with basic operations (+, -, ×, ÷)
- Precision control (decimal places)
- Theme switching (dark/light)
- Sound toggle
- Settings page with tab navigation
- Settings saved via `coconut.dialog.save()` and `coconut.fs.writeText()`

**Commands:**
- `calc_calculate` — Perform arithmetic
- `settings_save` — Save settings to file
- `settings_load` — Load settings from file

**Architecture:**

```
views/
├── app.html          # Vue app entry (loads from Vite dev server or dist/)
commands/
├── calc.lua          # Calculator logic
└── settings.lua      # Settings persistence
```

**Running:**

```bash
just run-vue
# Or manually:
cd examples/calculator-vue
coconut
```

---

### ocr-app

An image OCR scanner using Tesseract.js for text extraction.

**Location:** `examples/ocr-app/`

**Tech stack:**
- Alpine.js (reactive frontend)
- UnoCSS (utility-first CSS)
- Tesseract.js (OCR engine, runs in browser)

**Features:**
- Image upload via drag-and-drop or file picker
- OCR text extraction via Tesseract.js
- Dark theme design
- History tracking (client-side via `localStorage`)
- Settings tab with theme control
- Native save dialog for exporting results
- Window resize handling

**Commands:**
- `ocr_save_temp` — Save uploaded image to temp file
- `ocr_scan` — Run Tesseract OCR on image file

**Architecture:**

```
assets/
├── app.js            # Alpine.js component
├── app.css           # Custom styles (select, toggle, scrollbars)
└── lib/              # Vendored libraries
    ├── alpine.min.js
    ├── unocss-runtime.min.js
    └── unocss-reset.css
views/
└── scan.html         # Main view (scan + history + settings tabs)
commands/
└── ocr.lua           # File handling and OCR commands
```

**Running:**

```bash
just run-ocr
# Or manually:
cd examples/ocr-app
coconut
```

**OCR Pipeline:**

1. User drops image onto the page
2. `FileReader.readAsDataURL()` converts to base64
3. Frontend calls `coconut.call('ocr_save_temp', {name, data})`
4. Lua writes base64 to temp file, decodes via `base64 -d`
5. Frontend calls `coconut.call('ocr_scan', {image_path})`
6. Lua runs `tesseract - outputbase < image.png` and returns text

---

### code-editor

A full code editor with file tree, CodeMirror 6, syntax highlighting, and native dialogs.

**Location:** `examples/code-editor/`

**Tech stack:**
- CodeMirror 6 (editor engine)
- Thememirror (Dracula theme)
- CodeMirror language packages (Lua, JS, CSS, HTML, Markdown, Python, SQL, XML + legacy Lua/C-like)
- Alpine.js (sidebar component)
- UnoCSS (utility-first CSS)

**Features:**
- File tree sidebar (expand/collapse on-demand)
- CodeMirror 6 editor with syntax highlighting
- Dark theme (Dracula)
- Image preview via `<img src="file://...">`
- Native Open dialog (files + folders)
- Native Save / Save As dialogs
- Ctrl+S / Cmd+S keyboard shortcut
- Toolbar with Open / Save / Save As buttons

**Commands:**
- `editor_list_dir` — List directory contents (recursive)
- `editor_read_file` — Read file (text or image)
- `editor_save_file` — Write content to file
- `editor_open_dialog` — Show native open dialog (files + folders)
- `editor_save_dialog` — Show native save dialog

**Architecture:**

```
assets/
├── app.js            # Alpine.js + CodeMirror integration
├── app.css           # Editor styles (CM6 classes)
├── editor-bundle.js  # CodeMirror 6 IIFE bundle (built by bun)
└── lib/
    └── base64.lua    # Base64 encoding/decoding
views/
└── workspace.html    # Main view (sidebar + editor + toolbar)
commands/
└── editor.lua        # File operations and dialog handlers
```

**Running:**

```bash
just run-editor
# Or manually:
cd examples/code-editor
coconut
```

**CodeMirror 6 Bundle:**

The editor uses CodeMirror 6, bundled as an IIFE to avoid CORS issues:

```bash
cd examples/code-editor
bun run build-bundle
# Produces: assets/editor-bundle.js (IIFE, ~1.3 MB)
```

The bundle includes:
- `basicSetup` — Line numbers, syntax highlighting, undo/redo, close brackets
- 10 language packages — CSS, HTML, JavaScript (JSX/TS/TSX), JSON, Markdown, Python, SQL, XML + legacy Lua/C-like
- `thememirror` — Dracula theme
- Custom `coconutTheme` — Additional styling

---

### lua-html-app

A minimal app demonstrating the Lua HTML DSL for generating views without build tools.

**Location:** `examples/lua-html-app/`

**Tech stack:**
- Pure Lua HTML DSL (`lib/html.lua` — vendored from riki.house/lua-html, **not part of Coconut Milk core**)
- External CSS/JS via `coconut://` scheme

**Features:**
- Three views defined entirely via `html.div{}` syntax
- Navigation via `coconut://view_name` links
- No build step — all views generated at startup
- External CSS/JS loaded via `coconut://assets/style.css` and `coconut://assets/app.js`

**Caveats:**
- The DSL uses `ipairs` to render children, so tables with only string keys (pure layout nodes with no html.Element children) are silently dropped. Complex multi-pane layouts need careful structure.
- All pages must be part of a single view; `coconut://` scheme navigation between named views does not work from within a single lua-html generated page. For multi-page apps, use JS-side visibility toggling instead.

**Architecture:**

```
lib/
└── html.lua          # 117-line HTML DSL (vendored)
assets/
├── style.css         # Shared styles
└── app.js            # Frontend logic
views/
└── (none — views are generated in main.lua)
```

**How Views Are Generated:**

```lua
-- main.lua
local html = require("lib.html")

function coconut.views()
  return {
    home = View.html(tostring(html.div({ class = "app" },
      html.h1({}, "Lua HTML App"),
      html.nav({},
        html.a({ href = "coconut://about" }, "About"),
        html.a({ href = "coconut://contact" }, "Contact")
      ),
      html.p({}, "Generated entirely in Lua!")
    ))),
    about = View.html(tostring(html.div({ class = "app" },
      html.h1({}, "About"),
      html.p({}, "This view was generated by Lua.")
    ))),
    contact = View.html(tostring(html.div({ class = "app" },
      html.h1({}, "Contact"),
      html.p({}, "Email: hello@example.com")
    ))),
  }
end
```

**Running:**

```bash
just run-lua-html
# Or manually:
cd examples/lua-html-app
coconut
```

---

## Use Cases

### Multi-Page App with View Switching

Use `ctx:show()` to navigate between views:

```lua
-- main.lua
function coconut.views()
  return {
    home = View.load("views/home.html")
      :on_frontend_event("navigate", function(name, payload, ctx)
        if payload.view then
          ctx:show(payload.view)
        end
      end),
    settings = View.load("views/settings.html"),
    about = View.load("views/about.html"),
  }
end
```

```html
<!-- views/home.html -->
<nav>
  <a href="coconut://settings">Settings</a>
  <a href="coconut://about">About</a>
</nav>
```

### Settings Persistence

Save/load settings using native dialogs + filesystem:

```lua
-- commands/settings.lua
local settings = { theme = "dark", precision = 2 }

local function save(params, ctx)
  local result = coconut.dialog.save("Save Settings", "settings.json")
  if result.confirmed then
    local json_str = coconut.json.encode(settings)
    local ok = coconut.fs.writeText(result.path, json_str)
    return { ok = ok }
  end
  return { cancelled = true }
end

local function load(params, ctx)
  local result = coconut.dialog.open("Load Settings", false, false)
  if result.confirmed then
    local content = coconut.fs.readText(result.path)
    if content then
      settings = coconut.json.decode(content)
    end
  end
  return settings
end

return { save = save, load = load }
```

### File Tree with Recursive Listing

Build a file tree for a code editor:

```lua
-- commands/editor.lua
local function list_dir(payload, ctx)
  local dir = payload.path or "."
  return coconut.fs.listDir(dir)
  -- Returns: [{name, path, is_dir}, ...]
end

return { list_dir = list_dir }
```

```js
// Frontend: Build tree recursively
async function loadTree(dir = ".") {
  const items = await coconut.call("editor_list_dir", { path: dir })
  for (const item of items) {
    if (item.is_dir) {
      // Recurse into subdirectories
      item.children = await loadTree(item.path)
    }
  }
  return items
}
```

### Image Preview via File Path

Don't send binary data through the bridge — return the file path:

```lua
-- commands/editor.lua
local function read_file(payload, ctx)
  local path = payload.path
  local ext = path:match("%.([^%.]+)$")
  local image_exts = { png = true, jpg = true, jpeg = true, gif = true, svg = true }

  if image_exts[ext] then
    return { type = "image", path = path }
  end

  local content = coconut.fs.readText(path)
  return { type = "text", content = content }
end
```

```js
// Frontend: Load image via file:// URL
const result = await coconut.call("editor_read_file", { path })
if (result.type === "image") {
  editor.innerHTML = `<img src="file://${result.path}" />`
}
```

### Keyboard Shortcuts

Handle Cmd+S / Ctrl+S for save:

```js
// Frontend
document.addEventListener('keydown', (e) => {
  if ((e.metaKey || e.ctrlKey) && e.key === 's') {
    e.preventDefault()
    saveCurrent()
  }
})

async function saveCurrent() {
  const content = editorView.state.doc.toString()
  const result = await coconut.call("editor_save_file", {
    path: currentFilePath,
    content: content,
  })
  if (result.ok) {
    showToast('File saved')
  }
}
```

---

## Edge Cases

### Frameless Windows

Frameless windows on macOS require specific configuration:

```lua
function coconut.config(ctx)
  return ctx
    :setFrameless(true)
    :setTransparent(true)
    :setBackgroundColor(0, 0, 0, 0)
end
```

**How it works:**
- The title bar area becomes part of the window content area (the content view expands into it)
- The title text and traffic light buttons are hidden
- The result is a clean, chrome-free window that you can style entirely with CSS

**Limitation:** Frameless mode is macOS-only in v0.1. The window must keep its titled style mask internally — content view expansion is used instead of removing the title bit entirely.

### View.html() with Sub-resources

`View.html()` writes the HTML string to a **temp file** and navigates via `file://`. This ensures:

1. The navigation policy delegate fires for the initial load
2. Sub-resources (`<script>`, `<link>`) load with a proper base URL
3. `coconut://` URLs resolve correctly

**Example:**

```lua
View.html([[
  <!DOCTYPE html>
  <html>
  <head>
    <link rel="stylesheet" href="coconut://assets/style.css">
  </head>
  <body>
    <h1>Hello</h1>
    <script src="coconut://assets/app.js"></script>
  </body>
  </html>
]])
```

### ESM Modules from coconut://

**Problem:** `type="module"` scripts from `coconut://` are CORS-restricted when the page loads from `file://`. The webview silently discards them.

**Solution:** Bundle as IIFE and use plain `<script>` tags:

```bash
# Instead of ESM:
bun build lib/editor.mjs --outfile assets/editor-bundle.js --format iife
```

```html
<!-- Instead of: -->
<script type="module" src="coconut://assets/editor.mjs"></script>

<!-- Use: -->
<script src="coconut://assets/editor-bundle.js"></script>
```

### Dialog from Background Thread

Dialog calls are handled safely — native dialogs always run on the correct thread and exceptions are caught gracefully.

**Safety:** All dialog calls are wrapped in exception handling and `pcall` (Lua) to prevent crashes.

---

## Tips & Tricks

### Debug Mode

Enable verbose logging:

```bash
coconut --debug
```

Or via config:

```lua
return {
  debug = {
    enabled = true,
    showTransportDump = true,  -- Log all RPC messages
    logLevel = "debug",
  },
}
```

### Transport Logging

With `showTransportDump = true`, all bridge traffic is logged:

```
[DEBUG] transport: send → {"type":"call","id":"u1","name":"greet","payload":{"name":"Ada"}}
[DEBUG] transport: recv → {"type":"return","id":"u1","payload":{"greeting":"Hello, Ada!"}}
```

### FOUC Prevention

For UnoCSS runtime, prevent flash of unstyled content:

```html
<head>
  <style>[un-cloak]{display:none}</style>
  <link rel="stylesheet" href="coconut://assets/lib/unocss-reset.css">
</head>
<body>
  <div id="app" un-cloak>
    <!-- Content hidden until UnoCSS loads -->
  </div>
</body>
```

UnoCSS runtime removes the `un-cloak` attribute when styles are ready.

### Custom Select Dropdowns

Style `<select>` elements consistently:

```css
select {
  appearance: none;
  background-image: url("data:image/svg+xml,...");
  background-repeat: no-repeat;
  background-position: right 0.5rem center;
  padding-right: 2rem;
}

@supports (appearance: base-select) {
  /* Chrome 134+ progressive enhancement */
  select {
    appearance: base-select;
  }
}
```

### Window Resize Handling

Listen for window resize events:

```lua
-- main.lua
function coconut.on_resize(ctx, w, h)
  ctx:emit("window_resized", { w = w, h = h })
end
```

```js
// Frontend
coconut.on("window_resized", ({ w, h }) => {
  console.log(`Window resized to ${w}x${h}`)
  // Adjust layout, re-render charts, etc.
})
```

### Error Handling in Commands

Always use `pcall` for safety around native calls:

```lua
local function safe_read(params, ctx)
  local ok, result = pcall(coconut.fs.readText, params.path)
  if not ok then
    return { error = "Failed: " .. tostring(result) }
  end
  return { content = result }
end
```

### Logging from Lua

Use the built-in logging functions:

```lua
coconut.info("User opened file: " .. path)
coconut.warn("Deprecated command used: " .. name)
coconut.error("Failed to save: " .. tostring(err))
```

These use the same log level filtering as `debug::info()`, `debug::warn()`, `debug::error()`.

---

## Next Steps

- Read **[Troubleshooting](./troubleshooting.md)** for common errors
- Check the **[Roadmap](./roadmap.md)** for upcoming features
