---
layout: default
title: Keybind System
parent: Specifications
nav_order: 6
description: Hybrid platform+JS keybind registration with per-platform combos.
---

# Keybind System

Hybrid platform+JS keybind registration with per-platform combos, overrides,
and command palette integration.

---

## 1. Registration

### Lua

```lua
-- Simple: mod maps to cmd (macOS) / ctrl (others)
coconut.keybind("mod+s", function()
  ctx:emit({ name = "save" })
end, { id = "editor.save", scope = "editor" })

-- Bind a command name instead of a function
coconut.keybind("mod+shift+p", "editor_palette", { id = "app.palette" })

-- Explicit per-platform (overrides mod resolution)
coconut.keybind({
  mac   = "cmd+s",
  win   = "ctrl+s",
  linux = "ctrl+s",
}, fn)

-- Returns an unregister function
local unreg = coconut.keybind("mod+s", fn)
-- later: unreg()
```

### JavaScript

```js
coconut.keybind("mod+s", () => saveFile(), { id: "editor.save", scope: "editor" })

// Per-platform
coconut.keybind({ mac: "cmd+s", win: "ctrl+s" }, () => saveFile())

// Returns unregister
const unreg = coconut.keybind("mod+s", fn)
unreg()
```

---

## 2. Combo format

```
[modifiers+]key
```

### Modifiers

| Token | Platform mapping |
|---|---|
| `mod` | `cmd` on macOS, `ctrl` on Windows/Linux |
| `cmd` | `Meta` (macOS only) |
| `ctrl` | `Control` |
| `alt` | `Alt` |
| `shift` | `Shift` |

### Key values

- Letter/digit: `a`–`z`, `0`–`9`
- Symbols: `,`, `.`, `[`, `]`, `-`, `=`, etc.
- Named keys: `enter`, `tab`, `space`, `backspace`, `escape`
- Arrows: `up`, `down`, `left`, `right`
- Function: `f1`–`f20`
- OS keys: `cmd`, `ctrl`, `alt`, `shift` (as standalone)

### Examples

| Combo | macOS | Windows/Linux |
|---|---|---|
| `mod+s` | `cmd+s` | `ctrl+s` |
| `mod+shift+p` | `cmd+shift+p` | `ctrl+shift+p` |
| `alt+f4` | `alt+f4` | `alt+f4` |
| `mod+w` | `cmd+w` | `ctrl+w` (or `alt+f4` as close-tab on some platforms) |
| `escape` | `escape` | `escape` |

---

## 3. Dispatch chain

```
JS keydown event
  → JS registry lookup (early return if matched + preventDefault)
  → Platform NSEvent monitor (macOS: consume native combo before WebView sees it)
  → Bridge: emit "keydown" event to Lua
    → Lua registry lookup
      → If bound to function → call it
      → If bound to command name → ctx:bind dispatches
      → If not handled → fall through (optional pass-through to default browser behavior)
```

### Platform consumption (macOS)

On macOS, an `NSEvent` local monitor intercepts registered keybind combinations
**before** they reach the WebView. This prevents the system beep that occurs
when WKWebView receives unhandled key events for menus.

The platform handler:
1. Checks if the key combo matches a registered bind
2. If matched: consumes the event, dispatches to Lua → JS
3. If unmatched: lets the event pass through to the WebView

### Ordering

Keybinds registered in JS fire first (fastest path). If no JS handler matches,
the event propagates to Lua via a `"keydown"` bridge event. Lua handlers can
also prevent default.

---

## 4. Scopes

Keybinds can be scoped so they only fire when a particular view or context
is active.

```lua
coconut.keybind("mod+s", fn, { scope = "editor" })
coconut.keybind("mod+o", fn, { scope = "editor" })
coconut.keybind("mod+b", fn, { scope = "global" })  -- always active
```

| Scope | Behavior |
|---|---|
| `"global"` | Always active, regardless of current view |
| `"editor"` | Only active when the `editor` view is visible |
| *(any string)* | Scope is matched against the current view name |

### Scope activation

The runtime tracks the current active view. When a view becomes active, its
scoped keybinds are enabled. When the view changes, the old scope is disabled.

Scopes are a future refinement. The current implementation treats all keybinds
as global.

---

## 5. Overrides (dev-managed)

Keybinds can be overridden at runtime without re-registration, using a unique
`id` assigned at registration time.

```lua
-- Register with an id
coconut.keybind("mod+s", fn, { id = "editor.save" })

-- Override the combo
coconut.keybind.setOverride("editor.save", "ctrl+shift+s")

-- Restore default combo
coconut.keybind.clearOverride("editor.save")

-- Batch load overrides (e.g. from user settings)
local user_settings = json.decode(coconut.store:get("keybinds"))
coconut.keybind.loadOverrides(user_settings)

-- Query effective combo
local combo = coconut.keybind.getCombo("editor.save")
```

### Override storage

Overrides are stored in a `std::map<std::string, std::string>` (id → normalized combo)
on the native side. They are applied when dispatching: the effective combo is
checked before the default combo.

---

## 6. Per-platform defaults

Coconut ships with built-in per-platform keybind defaults:

| Action | macOS | Windows/Linux |
|---|---|---|
| Quit app | `mod+q` | `alt+f4` |
| Close tab/view | `mod+w` | `alt+f4` (docked), `ctrl+w` (windowed) |
| Toggle sidebar | `mod+\` | `ctrl+\` |
| Command palette | `mod+shift+p` | `ctrl+shift+p` |
| Reload view | `mod+r` | `ctrl+r` |
| Toggle dev tools | `mod+shift+i` | `ctrl+shift+i` |

These defaults are registered during app startup and can be overridden via
`coconut.keybind.setOverride()` or by re-registering with the same `id`.

---

## 7. Command palette

The command palette is a built-in view that lists all registered keybinds
(including their descriptions) and allows searching/filtering.

### Opening

- Default keybind: `mod+shift+p` / `ctrl+shift+p`
- Programmatic: `coconut.palette.open()`

### Palette view

Lists keybinds with:
- Keybind combo (formatted per-platform)
- Description (from `{ description = "..." }` at registration)
- Scope badge

Users can:
- Filter by combo or description
- Execute a command by selecting it
- See the effective combo (including overrides)

---

## 8. Keybind descriptions

Keybinds can carry a human-readable description, used in the command palette
and in auto-generated help overlays.

```lua
coconut.keybind("mod+s", fn, {
  id   = "editor.save",
  desc = "Save the current file",
})
```

```js
coconut.keybind("mod+s", fn, {
  id: "editor.save",
  desc: "Save the current file",
})
```

---

## 9. To be designed

- **Chord keybinds** — multi-step combos (e.g. `g g` in Vim)
- **Keybind groups** — enable/disable sets of keybinds
- **Global search** — from palette into commands, settings, and views
- **Platform keybind preferences** — macOS Preferences.expose bindings to System Settings
