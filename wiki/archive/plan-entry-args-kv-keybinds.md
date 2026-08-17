---
layout: default
title: Archived Plan — Entry Args, KV, Keybinds
parent: Archive
nav_order: 1
---

# Plan: Entry-point args, shared KV store, keybind management

## Final design decisions

### 1. Entry-point args — `coconut.args`

**Decision:** Global read-only table on both sides, not threaded through `coconut.config()`.

```
coconut myapp file.lua --mode=dev --verbose
```

```lua
coconut.args = {
  positional = {"file.lua"},
  named = { mode = "dev" },
  flags = { verbose = true }
}
```

- `argparse::Args` gets new fields: `positional_args`, `key_value_args`, `flag_args`
- Converted to `nlohmann::json` after parsing, stored on bridge state
- Lua: `coconut.args` → read-only `sol::table`
- JS: `coconut.args` injected during bridge init
- Backward-compatible (no signature changes to existing hooks)

---

### 2. Shared KV store — `coconut.store`

**Decision:** C++ `unordered_map<string, string>` as central source of truth, string-only values.

- Lua: `coconut.store:set(k,v) / :get(k) / :has(k) / :delete(k) / :clear() / :keys()` — sync
- JS: `coconut.store.set(k,v) / .get(k) / .has(k) / .delete(k)` — async via bridge commands
- Changes broadcast via `store:update` events (`{key, value}`) to the other side
- Both sides can `coconut.on('store:update', cb)` to react
- New module: `src/store.h` / `src/store.cpp`

---

### 3. Keybind management — `coconut.keybind`

**Decision:** Dual registry (JS + Lua), JS captures keydown first, then emits to Lua.

- Combo format: `ctrl+s`, `cmd+shift+p`, `alt+f4`, `esc`, `up`, `down`, `f1`..`f12`
- Scope: `"global"` (always active) or app-defined string
- JS: `coconut.keybind(combo, fn, scope?)` → returns unregister fn
- Lua: `coconut.keybind(combo, fn|cmd_name, scope?)` → returns unregister fn
- Dispatch: JS keydown → JS registry → emit `"keydown"` → Lua registry → fall through
- Event payload: `{key, ctrl, shift, alt, meta, scope}`


## Keybind API — refined

### Registration

```lua
-- Simple: mod auto-maps to cmd (macOS) / ctrl (others)
coconut.keybind("mod+s", fn, { id = "editor.save", scope = "editor" })

-- Explicit per-platform (overrides mod resolution)
coconut.keybind({
  mac  = "cmd+s",
  win  = "ctrl+s",
  linux = "ctrl+s",
}, fn, { id = "editor.save" })

-- Bind a command name instead of a function
coconut.keybind("mod+shift+p", "editor_palette", { id = "app.palette" })

-- JS side
coconut.keybind("mod+s", () => saveFile(), { id = "editor.save", scope = "editor" })
```

### Overrides (runtime, dev-managed)

```lua
-- Override a keybind's combo by id
coconut.keybind.setOverride("editor.save", "ctrl+shift+s")

-- Restore default combo
coconut.keybind.clearOverride("editor.save")

-- Batch load from a table (dev calls this from their own persistence)
local user_settings = json.decode(coconut.store:get("keybinds"))
coconut.keybind.loadOverrides(user_settings)

-- Query effective combo
local combo = coconut.keybind.getCombo("editor.save")
```

### Platform mapping

- `mod` → `cmd` on macOS, `ctrl` on Windows/Linux
- Explicit per-platform map overrides `mod` resolution
- Override API uses normalized combo (after mod resolution)
- JS does the same resolution at registration time
