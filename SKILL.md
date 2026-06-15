---
name: coconut-milk
description: Coconut Milk desktop app framework. Lua backend + HTML/CSS/JS frontend + native webview. Use when working on a coconut-milk project, writing Lua commands, authoring HTML views, configuring the bridge or keybinds, or generating TypeScript/JS wrappers from @command annotations.
---

# Coconut Milk Framework

## Quick start

```lua
-- main.lua
function coconut.config(ctx)
  return ctx:setWindowSize({ w = 1280, h = 640 }):setInitialView("home")
end

function coconut.views()
  return { home = View.load("views/home.html") }
end
```

```js
// Frontend
await coconut.ready()
const result = await coconut.call("greet", { name: "Ada" })
coconut.on("toast", (p) => showToast(p.message))
```

## Key concepts

- **`coconut.config(ctx)`** — required entry point, chainable context methods
- **`coconut.views()`** — named views (load/html/url)
- **Commands** — `ctx:bind()` or `---@command` in `commands/*.lua` + `coconut generate`
- **Events** — `ctx:emit()` (async), `ctx:emit_sync()` (blocking), `coconut.emit()` (frontend → Lua)
- **`coconut://`** — portable asset paths resolved from project root (macOS only)
- **Bridge** — RPC: `coconut.call()`, `coconut.ready()`, promise-based with error codes

## Common tasks

| Task | How |
|---|---|
| Add a Lua command | Write `---@command name` in `commands/foo.lua`, run `coconut generate` |
| Emit event to frontend | `ctx:emit("event_name", { key = "value" })` |
| Listen in JS | `coconut.on("event_name", (payload) => ...)` |
| Read/write files | `coconut.fs.readText(path)` / `coconut.fs.writeText(path, content)` |
| Show dialog | `coconut.dialog.open(title, multi_select, choose_dir)` |
| Switch view | `ctx:show("view_name")` |
| Quit | `coconut.window.close()` or `coconut.quit()` |

## Key gotchas

1. Call `coconut.ready()` before any `coconut.call()`
2. ESM (`type="module"`) scripts blocked from `coconut://` — use IIFE bundles
3. No binary data through bridge — return file paths instead
4. Payloads must be tables/objects, not raw strings
5. Always `pcall` around `coconut.dialog.*` calls
6. `coconut://` is macOS only (WKWebView scheme handler)
7. Frameless/transparent windows are macOS only
8. Lua is synchronous — long operations block the UI
9. Single window only — use `ctx:show()` for multi-page apps
10. `package.path` manipulations must end with `;`

## Full reference

See `.agents/skills/coconut-milk/skill-guide.md` for the complete API reference, patterns, and deeper documentation.

See `.agents/skills/coconut-milk/skill-quick-ref.md` for startup lifecycle, log levels, config format, and API cheatsheet.