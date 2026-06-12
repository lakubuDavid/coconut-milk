# Troubleshooting

Common errors, solutions, and debugging tips for Coconut Milk.

---

## Common Errors

### CommandNotFound

```json
{ "code": "CommandNotFound", "message": "No handler for 'my_command'" }
```

**Cause:** You called `coconut.call("my_command")` but the command is not registered.

**Fix:**

1. **Check the command file exists** in `commands/`:
   ```bash
   ls commands/
   # Should show: my_command.lua
   ```

2. **Check the @command annotation** matches the call name:
   ```lua
   -- commands/my_file.lua
   ---@command my_command  -- Must match the call name
   local function my_command(params, ctx)
     -- ...
   end
   ```

3. **Check the return table** exports the function:
   ```lua
   return { my_command = my_command }
   ```

4. **Run the generator** to create binding glue:
   ```bash
   coconut generate
   ```

5. **Check for duplicate names** — two commands with the same name will fail:
   ```lua
   -- This fails if "my_command" is already registered:
   ctx:bind("my_command", handler)
   -- Error: DuplicateCommand
   ```

---

### NotReady

```json
{ "code": "NotReady", "message": "Bridge not ready" }
```

**Cause:** You called `coconut.call()` before `coconut.ready()` resolved.

**Fix:** Always await `coconut.ready()` first:

```js
// ❌ Wrong — may fail if bridge is not ready
const result = await coconut.call("greet", { name: "Ada" })

// ✅ Correct — waits for bridge
await coconut.ready()
const result = await coconut.call("greet", { name: "Ada" })
```

---

### MissingFile

```json
{ "code": "MissingFile", "message": "File not found: views/home.html" }
```

**Cause:** A view file or asset doesn't exist at the expected path.

**Fix:**

1. **Check the file exists** relative to the app root:
   ```bash
   ls views/home.html
   ```

2. **Check the path resolution** — paths in `View.load()` are relative to the app root (CWD when `coconut` starts):
   ```lua
   -- If you're in /path/to/my-app:
   View.load("views/home.html")  -- → /path/to/my-app/views/home.html
   ```

3. **Check for typos** in the file path:
   ```lua
   -- ❌ Wrong:
   View.load("view/home.html")   -- Missing 's'

   -- ✅ Correct:
   View.load("views/home.html")
   ```

---

### DuplicateCommand

```json
{ "code": "DuplicateCommand", "message": "Command 'greet' is already registered" }
```

**Cause:** Two commands are registered with the same name.

**Fix:**

1. **Check auto-loaded commands** — `commands/` folder is scanned automatically:
   ```bash
   grep -r "@command greet" commands/
   ```

2. **Check manual bindings** — `ctx:bind()` in `coconut.commands()` or `coconut.config()`:
   ```lua
   -- If "greet" is auto-loaded from commands/greet.lua,
   -- this will fail:
   function coconut.commands(ctx)
     ctx:bind("greet", function() end)  -- Duplicate!
   end
   ```

3. **Rename one of the commands** to avoid conflict.

---

### LuaError

```json
{ "code": "LuaError", "message": "attempt to index a nil value" }
```

**Cause:** A Lua command threw an exception.

**Fix:**

1. **Check for nil values** in the command handler:
   ```lua
   -- ❌ Wrong — params may be empty
   local name = params.name  -- nil if params = {}

   -- ✅ Correct — use defaults
   local name = params.name or "World"
   ```

2. **Use pcall for safety:**
   ```lua
   local function safe_op(params, ctx)
     local ok, result = pcall(function()
       return coconut.fs.readText(params.path)
     end)
     if not ok then
       return { error = tostring(result) }
     end
     return { data = result }
   end
   ```

3. **Enable debug mode** for detailed error output:
   ```bash
   coconut --debug
   ```

---

## Dialog Crashes

### NSOpenPanel Crash

**Symptom:** App crashes when calling `coconut.dialog.open()`.

**Causes and fixes:**

1. **Init on factory result** — Fixed: `openPanel` returns an already-initialized instance. The code no longer calls `init` on it.

2. **Thread safety** — Fixed: Dialog calls are wrapped in `@try/@catch` in ObjC and `pcall` in Lua.

3. **Missing accessibility permissions** — If the dialog doesn't appear:
   - Go to **System Settings → Privacy & Security → Accessibility**
   - Add your terminal app (or the `coconut` binary) to the allowed list

### NSSavePanel Crash

Same fixes as NSOpenPanel. The code is now wrapped in `@try/@catch`:

```cpp
// In src/platform/darwin/dialog.mm
Result platformSaveFile(...) {
  Result result{};
  @try {
    // ObjC runtime calls
  } @catch (NSException *e) {
    debug::error(std::format("ObjC exception: {}", [[e reason] UTF8String]));
  } @catch (...) {
    debug::error("unknown exception");
  }
  return result;
}
```

### Lua pcall Safety

All example commands use `pcall` for safety:

```lua
-- In examples/code-editor/commands/editor.lua
local function open_dialog(payload, ctx)
  local ok, result = pcall(coconut.dialog.open, "Open File or Folder", false, true)
  if not ok then
    coconut.error("open_dialog: pcall failed: " .. tostring(result))
    return { cancelled = true, error = tostring(result) }
  end
  -- ...
end
```

---

## CORS / ESM Module Issues

### Scripts Not Loading from coconut://

**Symptom:** `<script src="coconut://assets/app.js">` loads fine, but `<script type="module" src="coconut://assets/app.mjs">` silently fails.

**Cause:** `type="module"` ESM scripts are CORS-restricted. The page loads from `file://` but scripts load from `coconut://` — different origins. WKWebView silently discards cross-origin ESM modules.

**Fix:** Bundle as IIFE and use plain `<script>` tags:

```bash
# Bundle as IIFE instead of ESM:
bun build lib/editor.mjs --outfile assets/editor-bundle.js --format iife
```

```html
<!-- Use plain script tag, no type="module" -->
<script src="coconut://assets/editor-bundle.js"></script>
```

### CSS Not Loading

**Symptom:** `<link rel="stylesheet" href="coconut://assets/style.css">` doesn't apply.

**Cause:** Check that you're using `<link rel="stylesheet">`, not `<script>`:

```html
<!-- ❌ Wrong — CSS is a stylesheet, not a script -->
<script src="coconut://assets/style.css"></script>

<!-- ✅ Correct -->
<link rel="stylesheet" href="coconut://assets/style.css">
```

### UnoCSS Reset

UnoCSS reset must be loaded as a stylesheet, not a script:

```html
<head>
  <link rel="stylesheet" href="coconut://assets/lib/unocss-reset.css">
</head>
```

---

## Debugging

### Enable Debug Mode

```bash
coconut --debug
```

This enables verbose logging including:
- Window hierarchy dumps
- Bridge message details
- View loading steps
- Command registration

### Transport Logging

Dump all RPC messages:

```lua
-- coconut.config.lua
return {
  debug = {
    enabled = true,
    showTransportDump = true,  -- Log every message
    logLevel = "debug",
  },
}
```

Output:

```
[DEBUG] transport: send → {"type":"call","id":"u1","name":"greet","payload":{"name":"Ada"}}
[DEBUG] transport: recv → {"type":"return","id":"u1","payload":{"greeting":"Hello, Ada!"}}
```

### WebKit Inspector

Enable the WebKit inspector for frontend debugging:

```lua
-- In coconut.config.lua
return {
  debug = {
    enabled = true,
  },
}
```

Then in the webview, right-click and select "Inspect Element" (macOS).

### Lua Logging

Use the built-in logging functions:

```lua
coconut.info("Info message")     -- Cyan [INFO]
coconut.warn("Warning message")  -- Yellow [WARN]
coconut.error("Error message")   -- Red [ERROR]
coconut.debug("Debug message")   -- Grey [DEBUG] (only with --debug flag)
```

**Log levels:**

| Level | Shows |
|---|---|
| `debug` | All messages |
| `info` | `[INFO]`, `[WARN]`, `[ERROR]` (default) |
| `warn` | `[WARN]`, `[ERROR]` |
| `error` | Only `[ERROR]` |

### Frontend Console Logs

Use `console.log()` in the frontend. Logs appear in the WebKit inspector console.

```js
console.log('Frontend loaded')
console.log('Command result:', result)
console.error('Command failed:', err)
```

---

## Platform Issues

### macOS: Frameless Window Not Working

**Check:**

1. **Window style is applied AFTER Lua config** — `applyWindowStyle()` runs after `loadEntryPoint()`, so `coconut.config(ctx)` overrides work correctly.

2. **Use the correct approach** — Don't try to remove `NSWindowStyleMaskTitled` after the window is displayed. Use `NSFullSizeContentViewWindowMask` instead:

   ```lua
   function coconut.config(ctx)
     return ctx
       :setFrameless(true)
       :setTransparent(true)
   end
   ```

3. **Traffic light buttons** — Hidden via view hierarchy traversal, not `standardWindowButton:` (which returns nil on webview-created windows).

### macOS: Transparent Background Not Working

**Check:**

1. **Both flags must be set:**
   ```lua
   ctx:setFrameless(true)
   ctx:setTransparent(true)
   ctx:setBackgroundColor(0, 0, 0, 0)
   ```

2. **Frontend CSS class** — When `transparent` is true, the frontend receives a `transparent-window` CSS class on `<body>`:

   ```css
   body.transparent-window {
     background: transparent;
   }
   ```

### Windows/Linux: coconut:// Not Working

The `coconut://` scheme handler is implemented only for macOS (WKWebView) in v0.1. Windows and Linux are stubs.

**Workaround:** Use absolute `file://` URLs for assets on Windows/Linux:

```lua
-- Instead of:
View.load("views/home.html")

-- On Windows/Linux, ensure paths are absolute:
local abs_path = "/absolute/path/to/views/home.html"
View.load(abs_path)
```

### Windows/Linux: Frameless Not Working

Frameless window support is macOS-only in v0.1. Windows and Linux use the default window chrome.

---

## Build Issues

### xmake Build Fails

**Check:**

1. **xmake is installed:**
   ```bash
   xmake --version
   ```

2. **C++20 toolchain is available:**
   ```bash
   clang++ --version   # macOS
   g++ --version       # Linux
   ```

3. **Clean and rebuild:**
   ```bash
   just clean
   just build
   ```

### Justfile Recipe Fails

**Check:**

1. **Binary path resolves correctly:**
   ```bash
   ls build/macosx/x86_64/debug/coconut
   ```

2. **Example directory exists:**
   ```bash
   ls examples/code-editor/
   ```

3. **Run from project root:**
   ```bash
   cd /path/to/coconut-milk
   just run-editor
   ```

### create-coconut-app Fails

**Check:**

1. **LuaJIT is installed:**
   ```bash
   luajit --version
   ```

2. **Run with explicit path:**
   ```bash
   /path/to/scripts/create-coconut-app my-app
   ```

---

## Next Steps

- Check the **[Examples](./examples.md)** page for working patterns
- Read the **[Roadmap](./roadmap.md)** for upcoming fixes
- Open an issue on [GitHub](https://github.com/lakubuDavid/coconut-milk/issues)
