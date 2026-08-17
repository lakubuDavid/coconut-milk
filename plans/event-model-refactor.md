# Event Model Refactor — Migration Plan

Unify event dispatch with a DOM-like event object. Replace the current
`(name, payload, ctx)` triple with a single event table.

---

## Design decisions

See todo list for full decision log (`plans/todo/`).

### Event object shape

```lua
{
  name = "resize",            -- event name
  type = "resize",            -- getter → name
  target = "editor",          -- view name string ("" for global)
  -- payload fields merged in directly:
  w = 1024,
  h = 768,

  -- Methods (via metatable)
  preventDefault = fn,         -- sets defaultPrevented = true
  stopPropagation = fn,        -- sets propagationStopped = true
  stopImmediatePropagation = fn, -- sets both
  defaultPrevented = false,    -- flag, readonly
  propagationStopped = false,  -- flag, readonly
}
```

### Dispatch chain (Lua)

```
coconut._dispatch(name, payload, target)
  │
  ├─ 1. view:on(name, fn)          ← active view's callbacks
  │     stopPropagation → skip rest
  │
  ├─ 2. coconut.on(name, fn)       ← global subscribe (FIFO)
  │     stopPropagation → skip rest
  │
  └─ 3. coconut.events(event)      ← if-else fallback (always)
        preventDefault → sets flag

After chain: if defaultPrevented && event is cancellable → veto default
```

### JS dispatch chain

```
CoconutEvent reconstructed
  └─ coconut.on(name, fn)          ← subscribe only, no events() fallback
```

### Key API changes

| Old | New |
|---|---|
| `ctx:emit("name", payload)` | `ctx:emit({name="name", ...payload})` |
| `ctx:emit_sync(name, payload)` | `ctx:emit_sync({name, ...})` |
| `coconut.events(name, payload, ctx)` | `coconut.events(event)` |
| `view:on_frontend_event(name, fn)` | `view:on(name, fn)` |
| `coconut.on_resize(ctx, w, h)` | `coconut.on("resize", fn(event))` |
| `coconut.on_ready(ctx)` | `coconut.on("ready", fn(event))` |
| `coconut.on_close(ctx)` | `coconut.on("close", fn(event))` |
| `coconut.on_focus()` / `on_blur()` | `coconut.on("focus", fn)` / `on("blur", fn)` |
| — | **NEW:** `coconut.on(name, fn, {once=true})` → unregister |
| `coconut.emit(name, payload)` (JS) | `coconut.emit({name, ...})` (JS) |

---

## Migration stages

### Stage 0 — Prepare test infrastructure

**Goal:** Test factories and helpers so we can validate each step.

Files to create/modify:

- `tests/helpers/event.lua` — `makeEvent(name, payload, target)` factory
- `tests/helpers/mock_context.lua` — mock CoconutContext for dispatch tests
- `tests/unit/event_dispatch_test.lua` — test the new dispatch chain
- Update `tests/test.h` if new C++ test helpers are needed

Acceptance:
```
coconut-milk-tests passes with event helper tests
```

---

### Stage 1 — Migrate examples & samples

**Goal:** All Lua examples use the new event object API. They will break until
the runtime catches up — that's intentional. Each broken example defines what
the runtime must implement.

Files to migrate (Lua):

| File | Changes needed |
|---|---|
| `examples/code-editor/main.lua` | `coconut.events(name,p,ctx)` → `coconut.events(event)` |
| `examples/playground/main.lua` | same |
| `examples/lua-html-app/main.lua` | same |
| `samples/main.lua` | same |
| `samples/sample.lua` | same |
| `samples/coconut.d.lua` | `on_frontend_event` → `on` in type defs |
| `test-x/` | same pattern (scaffold test) |

Files to migrate (JS):

| File | Changes needed |
|---|---|
| `src/embeds/coconut.ts` | `coconut.emit(name, payload)` → `coconut.emit({name, ...})` |
| `src/embeds/coconut.js` | compiled from TS |
| `examples/code-editor/assets/app.js` | JS-side emit calls |

Acceptance:
```
All examples use ctx:emit({name, ...}) syntax.
Build completes. Runtime still uses old dispatch — examples fail at runtime.
```

---

### Stage 2 — Implement core dispatch (Lua side)

**Goal:** `coconut._dispatch` works end-to-end. Examples start passing.

#### Step 2a — Event helper factory

Implement `coconut._makeEvent(name, payload, target)` in the Lua runtime
(`lua_runtime.cpp` View module setup region).

```lua
function coconut._makeEvent(name, payload, target)
  local event = setmetatable({
    name = name,
    target = target or "",
    defaultPrevented = false,
    propagationStopped = false,
  }, {
    __index = {
      type = name,  -- getter proxying to name
      preventDefault = function(self)
        self.defaultPrevented = true
      end,
      stopPropagation = function(self)
        self.propagationStopped = true
      end,
      stopImmediatePropagation = function(self)
        self.defaultPrevented = true
        self.propagationStopped = true
      end,
    }
  })
  for k, v in pairs(payload or {}) do
    if k ~= "name" and k ~= "type" then
      event[k] = v
    end
  end
  return event
end
```

#### Step 2b — `coconut.on()` subscribe API

Add `coconut.on(name, fn, opts)` → unregister function.

```lua
coconut._listeners = coconut._listeners or {}

function coconut.on(name, fn, opts)
  opts = opts or {}
  if not coconut._listeners[name] then
    coconut._listeners[name] = {}
  end
  local entry = { fn = fn, once = opts.once == true }
  table.insert(coconut._listeners[name], entry)
  return function()
    for i, e in ipairs(coconut._listeners[name]) do
      if e == entry then
        table.remove(coconut._listeners[name], i)
        return
      end
    end
  end
end
```

#### Step 2c — `view:on()` rename

In the View descriptor metatable:

```lua
on = function(self, name, fn)
  self._callbacks[name] = fn
  return self
end
```

Remove `on_frontend_event`. Update `_callbacks.frontend_events[name]` storage
to flat `_callbacks[name]`.

#### Step 2d — `coconut._dispatch()` central dispatcher

```lua
function coconut._dispatch(name, payload, target)
  local event = coconut._makeEvent(name, payload, target)

  -- Tier 1: active view's on(name, fn)
  if target and target ~= "" then
    local view = coconut._view_descriptors[target]
    if view and view._callbacks and view._callbacks[name] then
      view._callbacks[name](event)
      if event.propagationStopped then return event end
    end
  end

  -- Tier 2: global coconut.on(name, fn) — FIFO
  local listeners = coconut._listeners[name]
  if listeners then
    -- Snapshot in case a listener unregisters itself
    local i = 1
    while i <= #listeners do
      local entry = listeners[i]
      entry.fn(event)
      if entry.once then
        table.remove(listeners, i)
      else
        i = i + 1
      end
      if event.propagationStopped then return event end
    end
  end

  -- Tier 3: coconut.events(event) fallback
  if coconut.events then
    coconut.events(event)
  end

  return event
end
```

#### Step 2e — `ctx:emit()` rewrite

Currently a C++ function that calls `bridge::emitToJS()`. Rewrite to:

1. Accept a single table arg (event object)
2. Call `coconut._dispatch(event.name, event, target="")`
3. Call `bridge::emitToJS(app, event.name, payloadJson)` to forward to JS
4. Support backward compat? Decide: no — old `ctx:emit(name, payload)` removed.

```cpp
// New emit binding
ctx_table.set_function("emit", [runtime](sol::table event) {
  std::string name = event["name"];
  // Extract payload (everything except name/type/target/methods)
  nlohmann::json payloadJson = tableToJson(event, {"name", "type", "target",
    "preventDefault", "stopPropagation", "stopImmediatePropagation"});
  // Lua-side dispatch
  if (runtime->app->lua_state) {
    sol::state_view lua(*runtime->app->lua_state->lua_state);
    sol::function dispatch = lua["coconut"]["_dispatch"];
    if (dispatch.valid()) {
      dispatch(name, payloadJson, "");  // target="" from Lua context
    }
  }
  // JS-side dispatch
  bridge::emitToJS(runtime->app, name, payloadJson);
});
```

#### Step 2f — `ctx:emit_sync()` rewrite

Same as `emit()` but with synchronous delivery semantics. For v1, `emit_sync`
is identical to `emit` (the sync distinction is a future concern).

#### Step 2g — Update C++ `dispatchEventToLua()`

Currently `src/bridge.cpp`:

```cpp
void dispatchEventToLua(App* app, const std::string& name,
                         const nlohmann::json& payload) {
  // ...look up coconut.events(name, payloadTable, ctx)...
}
```

New version builds an event table and calls `coconut._dispatch`:

```cpp
void dispatchEventToLua(App* app, const std::string& name,
                         const nlohmann::json& payload) {
  if (!app || !app->lua_state || !app->lua_state->lua_state) return;
  // Remove keydown special-casing — handled by _dispatch now
  sol::state_view lua(*app->lua_state->lua_state);
  sol::function dispatch = lua["coconut"]["_dispatch"];
  if (!dispatch.valid()) return;
  std::string target = app->window ? app->window->current_view : "";
  dispatch(name, payload, target);
}
```

**No more special-casing for "keydown"** — keybinds register via
`coconut.on("keydown", fn)` like everything else.

Acceptance:
```
coconut-milk-tests passes.
All examples run without crashing.
coconut.on("resize", fn) fires on window resize.
view:on("resize", fn) fires for active view.
stopPropagation() works across tiers.
preventDefault() sets flag (noop for non-cancellable events).
```

---

### Stage 3 — Migrate platform lifecycle dispatch

**Goal:** Lifecycle events (resize, focus, blur) flow through `coconut._dispatch`.

#### Step 3a — Register lifecycle events via `coconut.on()`

The macOS lifecycle observer currently calls `dispatch("resize", {w,h})`
which calls `bridge::emitToJS()` + `bridge::dispatchEventToLua()`.

After Stage 2, `bridge::dispatchEventToLua()` already routes through
`coconut._dispatch`. So this step is already done — just remove the old
`coconut.events(name, p, ctx)` call from the dispatch path.

#### Step 3b — Remove `coconut.events` as low-level dispatcher

Keep `coconut.events(event)` as tier 3 fallback. Remove the old C++ code
that special-cased `keydown` events (now they go through `_dispatch`).

#### Step 3c — Add lifecycle-specific cancellable events

- `"close"` event — cancellable via `preventDefault()`. If default prevented,
  `webview_terminate()` is not called.
- `"ready"` event — fires once after initial view mount, before event loop.
  Non-cancellable.

Acceptance:
```
Window resize → coconut.on("resize", fn) fires.
Window focus → coconut.on("focus", fn) fires.
Window close → coconut.on("close", fn) fires → preventDefault() vetoes close.
coconut.on("ready", fn) fires at startup.
```

---

### Stage 4 — JS side implementation

**Goal:** JS event model mirrors Lua (minus the events() fallback).

#### Step 4a — `CoconutEvent` class

In `src/embeds/coconut.ts`:

```ts
class CoconutEvent {
  readonly name: string
  readonly target: string
  defaultPrevented = false
  propagationStopped = false
  [key: string]: unknown

  constructor(name: string, payload: Record<string, unknown>, target: string) {
    this.name = name
    this.target = target
    Object.assign(this, payload)
  }

  get type(): string { return this.name }

  preventDefault(): void { this.defaultPrevented = true }
  stopPropagation(): void { this.propagationStopped = true }
  stopImmediatePropagation(): void {
    this.defaultPrevented = true
    this.propagationStopped = true
  }
}
```

#### Step 4b — `coconut.on()` for JS

```ts
type ListenerEntry = { fn: (event: CoconutEvent) => void, once: boolean }
const listeners: Record<string, ListenerEntry[]> = {}

function on(name: string, fn: (event: CoconutEvent) => void, opts?: { once?: boolean }): () => void {
  if (!listeners[name]) listeners[name] = []
  const entry: ListenerEntry = { fn, once: opts?.once ?? false }
  listeners[name].push(entry)
  return () => {
    const idx = listeners[name].indexOf(entry)
    if (idx >= 0) listeners[name].splice(idx, 1)
  }
}
```

#### Step 4c — `coconut.emit()` for JS (sends to Lua)

```ts
async function emit(event: Record<string, unknown>): Promise<void> {
  const name = event.name as string
  if (!name) throw new Error("event must have a 'name' field")
  // Forward to Lua via bridge
  await __coconut_rpc_send({ type: "event", name, payload: JSON.stringify(event) })
}
```

#### Step 4d — Incoming event dispatch (Lua → JS)

When `__coconut_dispatch_event(name, payloadJson)` is called from C++:

```ts
function __coconut_dispatch_event(name: string, payloadJson: string, target: string) {
  const payload = JSON.parse(payloadJson)
  const event = new CoconutEvent(name, payload, target)

  // Dispatch to coconut.on() listeners
  const entries = listeners[name]
  if (entries) {
    let i = 0
    while (i < entries.length) {
      const entry = entries[i]
      entry.fn(event)
      if (entry.once) entries.splice(i, 1)
      else i++
      if (event.propagationStopped) return
    }
  }
}
```

Acceptance:
```
JS coconut.on("resize", fn) fires on resize.
JS coconut.emit({name: ...}) sends event to Lua.
stopPropagation() works on JS side.
```

---

### Stage 5 — Deprecate and remove old paths

**Goal:** Clean up dead code.

- Remove `coconut.events(name, payload, ctx)` triple-arg overload
- Remove old `dispatchEventToLua` with string dispatching
- Remove special `keydown` handling in C++ (now in Lua `_dispatch`)
- Remove `on_frontend_event` from view descriptors
- Remove `coconut.on_resize` etc global hook stubs (replaced by `coconut.on()`)

Acceptance:
```
No dead code. grep for old patterns returns empty.
All tests pass. All examples work.
```

---

## Files affected (complete inventory)

### C++ — src/

| File | Change |
|---|---|
| `src/bridge.cpp` | `dispatchEventToLua()` → calls `coconut._dispatch` |
| `src/bridge.h` | Update signature if needed |
| `src/lua_runtime.cpp` | `ctx:emit()` binding, View module metatable (`on_frontend_event` → `on`), add `_makeEvent`, `_dispatch`, `on()` |
| `src/context.cpp` | `CoconutWindowHandle` — ensure view switching dispatch still works |
| `src/main.cpp` | `coconut.on("ready")` fires after mount |
| `src/platform/darwin/lifecycle.cpp` | Remove old dispatch, events flow through bridge → `_dispatch` |
| `src/platform/darwin/keyboard.mm` | May simplify — keydown events route through `_dispatch` |

### Lua — examples/ samples/

| File | Change |
|---|---|
| `examples/code-editor/main.lua` | `coconut.events(event)` |
| `examples/playground/main.lua` | same |
| `examples/lua-html-app/main.lua` | same |
| `samples/main.lua` | same |
| `samples/sample.lua` | same |
| `samples/coconut.d.lua` | types: `on_frontend_event` → `on` |

### TypeScript / JS — src/embeds/

| File | Change |
|---|---|
| `src/embeds/coconut.ts` | CoconutEvent class, `coconut.on()`, `coconut.emit()` |
| `src/embeds/coconut.js` | Regenerated from TS |

### Tests

| File | Change |
|---|---|
| `tests/helpers/event.lua` | NEW: event factory |
| `tests/unit/event_dispatch_test.lua` | NEW: dispatch chain tests |
| `tests/unit/bridge_test.cpp` | Update if dispatch tests reference old API |

### Docs

| File | Change |
|---|---|
| `wiki/reference/specs/specs.md` | Full event model rewrite (§3,4,5,8,11,12) |
| `wiki/reference/api-reference.md` | All emit/events/on signatures |
| `wiki/reference/lua-guide.md` | Event object usage |
| `wiki/decisions/concepts.md` | Event system concepts |
| `wiki/examples/index.md` | Example code snippets |
| `wiki/reference/bridge.md` | RPC envelope unchanged (name stays separate) |
| `wiki/guides/getting-started.md` | Quick-start snippets |

---

## Rollback plan

Each stage is independently revertible via git:

```
Stage 0: git checkout origin/main -- tests/helpers/ tests/unit/event_dispatch_test.lua
Stage 1: git checkout origin/main -- examples/ samples/
Stage 2: git checkout origin/main -- src/lua_runtime.cpp src/bridge.cpp
Stage 3: git checkout origin/main -- src/platform/ src/main.cpp
Stage 4: git checkout origin/main -- src/embeds/
Stage 5: git checkout origin/main -- docs/
```

No stage depends on a later stage. If Stage 2 breaks, Stage 1 (examples)
can still be reverted independently.
