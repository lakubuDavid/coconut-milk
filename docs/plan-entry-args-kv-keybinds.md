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
