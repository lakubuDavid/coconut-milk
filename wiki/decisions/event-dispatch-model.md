---
layout: default
title: Event Dispatch Model
parent: Explanation
nav_order: 2
description: Deep dive into event routing, queues, and dispatch mechanics.
---

# Event Dispatch Model

This document explains the architecture and internals of Coconut Milk's event system, including the three-tier dispatch chain, event object structure, and propagation control.

---

## Overview

Coconut Milk uses a DOM-inspired event model that provides flexible, hierarchical event handling across Lua and JavaScript. Events flow through three tiers, allowing fine-grained control over event propagation and handling.

**Key concepts:**
- Events are Lua tables with a metatable providing methods
- Three-tier dispatch chain: view → global → fallback
- Propagation control via `preventDefault()` and `stopPropagation()`
- Bidirectional: Lua ↔ JavaScript via RPC bridge

---

## Event object structure

### Lua event objects

Events in Lua are plain tables with a metatable that provides methods:

```lua
local event = {
  name = "resize",
  target = "editor",
  defaultPrevented = false,
  propagationStopped = false,
  w = 1024,  -- payload field
  h = 768    -- payload field
}

-- Metatable provides methods
setmetatable(event, {
  __index = {
    preventDefault = function(self)
      self.defaultPrevented = true
    end,
    stopPropagation = function(self)
      self.propagationStopped = true
    end,
    stopImmediatePropagation = function(self)
      self.defaultPrevented = true
      self.propagationStopped = true
    end
  }
})
```

**Properties:**
- `name` (string) — Event name, e.g., "resize", "click", "save"
- `target` (string) — View name or empty string for global events
- `defaultPrevented` (boolean) — Set by `preventDefault()`
- `propagationStopped` (boolean) — Set by `stopPropagation()`
- Payload fields — Merged directly into event table (e.g., `w`, `h`)

**Methods:**
- `preventDefault()` — Marks event as cancelled (for cancellable events)
- `stopPropagation()` — Prevents event from reaching next tier
- `stopImmediatePropagation()` — Both of the above

---

### JavaScript event objects

Events in JavaScript are plain objects reconstructed from RPC messages:

```js
{
  name: "resize",
  target: "editor",
  defaultPrevented: false,
  propagationStopped: false,
  w: 1024,
  h: 768,
  
  // Methods
  preventDefault() { this.defaultPrevented = true },
  stopPropagation() { this.propagationStopped = true },
  stopImmediatePropagation() {
    this.defaultPrevented = true
    this.propagationStopped = true
  }
}
```

**Note:** JavaScript events are reconstructed from RPC messages, so they don't share object identity with Lua events.

---

## Three-tier dispatch chain

The diagram below illustrates how events flow through the three tiers of the dispatch system:

<p align="center">
  <img src="../diagrams/event-dispatch-chain.png" alt="Three-tier event dispatch chain diagram" width="800">
</p>

<br>

When an event is emitted, it flows through three tiers in order:

```
1. View scope      → view:on(name, fn) fires if active view has listener
2. Global scope    → coconut.on(name, fn) listeners fire in FIFO order
3. Fallback        → coconut.events(event) fires last
```

### Tier 1: View-scoped handlers

View-scoped handlers are registered via `view:on()` and only fire when the associated view is active.

```lua
local editor = View.load("views/editor.html")
  :on("save", function(event)
    print("Editor save handler")
    saveFile()
  end)
  :on("format", function(event)
    print("Editor format handler")
    formatCode()
  end)
```

**Behavior:**
- Only fires if the view is currently active
- Fires before global handlers
- Can stop propagation to prevent global handlers from firing
- Stored in `view._callbacks` table

**Use cases:**
- View-specific logic (e.g., editor save, form validation)
- Preventing global handlers from running in certain views
- Encapsulating view behavior

---

### Tier 2: Global handlers

Global handlers are registered via `coconut.on()` and fire for all events with matching names, regardless of active view.

```lua
local unsub1 = coconut.on("save", function(event)
  print("Global save handler 1")
  logSaveEvent(event)
end)

local unsub2 = coconut.on("save", function(event)
  print("Global save handler 2")
  updateRecentFiles(event.path)
end)
```

**Behavior:**
- Fires for all events with matching name
- Multiple handlers fire in registration order (FIFO)
- Can stop propagation to prevent fallback handler from firing
- Stored in `coconut._listeners[name]` as array of `{ fn, once }` entries

**Use cases:**
- Application-wide event handling (e.g., logging, analytics)
- Cross-cutting concerns (e.g., state management, persistence)
- Multiple independent handlers for the same event

**One-time handlers:**

```lua
coconut.on("app_ready", function(event)
  print("App ready, initializing...")
  initializeApp()
end, { once = true })  -- Automatically unsubscribes after first call
```

---

### Tier 3: Fallback handler

The fallback handler is a single function assigned to `coconut.events` that fires for all events that reach it.

```lua
function coconut.events(event)
  print("Fallback handler for:", event.name)
  
  if event.name == "unknown_event" then
    handleUnknownEvent(event)
  end
end
```

**Behavior:**
- Fires last, after all view and global handlers
- Only fires if propagation wasn't stopped
- Single function (overwritten if reassigned)
- Acts as catch-all for unhandled events

**Use cases:**
- Debug logging of unhandled events
- Default behavior for unknown events
- Centralized event routing

---

## Dispatch algorithm

The dispatch algorithm is implemented in Lua as `coconut._dispatch`:

```lua
function coconut._dispatch(name, payload, target)
  -- Create event object with metatable
  local event = createEvent(name, payload, target)
  
  -- Tier 1: View-scoped handlers
  local view = coconut._view_descriptors[target]
  if view and view._callbacks and view._callbacks[name] then
    view._callbacks[name](event)
    if event.propagationStopped then
      return event
    end
  end
  
  -- Tier 2: Global handlers
  local listeners = coconut._listeners[name]
  if listeners then
    for _, entry in ipairs(listeners) do
      entry.fn(event)
      if entry.once then
        -- Remove one-time listener
        removeListener(name, entry)
      end
      if event.propagationStopped then
        return event
      end
    end
  end
  
  -- Tier 3: Fallback handler
  if coconut.events and type(coconut.events) == "function" then
    coconut.events(event)
  end
  
  return event
end
```

**Key points:**
- Event object is created once and passed through all tiers
- Propagation can be stopped at any tier
- One-time listeners are removed after firing
- Fallback handler only fires if propagation wasn't stopped

---

## Propagation control

### stopPropagation()

Stops the event from reaching the next tier.

```lua
-- View handler stops propagation
local editor = View.load("views/editor.html")
  :on("save", function(event)
    saveFile()
    event:stopPropagation()  -- Global handlers won't fire
  end)

-- Global handler (won't fire if view handler stops propagation)
coconut.on("save", function(event)
  print("This won't fire")
end)
```

**Use cases:**
- View-specific handling that should override global behavior
- Preventing default behavior in certain contexts
- Event consumed by handler

**Important:** Does not prevent other handlers in the same tier from firing.

---

### preventDefault()

Marks the event as cancelled. Only meaningful for cancellable events.

```lua
coconut.on("close", function(event)
  if hasUnsavedChanges() then
    event:preventDefault()  -- Cancel the close
    promptSaveBeforeQuit()
  end
end)
```

**Cancellable events:**
- `close` — App close can be vetoed

**Use cases:**
- Vetoing app close when unsaved changes exist
- Preventing default behavior for custom events

**Important:** Does not stop propagation. Other handlers still fire.

---

### stopImmediatePropagation()

Combines both `preventDefault()` and `stopPropagation()`.

```lua
coconut.on("error", function(event)
  handleCriticalError(event)
  event:stopImmediatePropagation()  -- Stop everything
end)
```

**Use cases:**
- Critical errors that should halt all processing
- Security-sensitive events that must be handled exclusively

---

## Event emission

### Lua → JavaScript

Use `ctx:emit()` to emit events from Lua to JavaScript:

```lua
ctx:emit({ 
  name = "user_login",
  username = "alice",
  role = "admin"
})
```

**Flow:**
1. Lua creates event table
2. Event flows through three-tier dispatch chain in Lua
3. Event is serialized to JSON
4. RPC message sent to JavaScript
5. JavaScript reconstructs event object
6. JavaScript listeners are notified

**Implementation:**

```lua
function CoconutContext:emit(event)
  -- Run Lua dispatch chain
  coconut._dispatch(event.name, event, self.active_view)
  
  -- Forward to JavaScript via RPC
  bridge.emit(self.app, event.name, event)
end
```

---

### JavaScript → Lua

Use `coconut.emit()` to emit events from JavaScript to Lua:

```js
coconut.emit({ 
  name: "navigate",
  view: "settings"
})
```

**Flow:**
1. JavaScript creates event object
2. Event is serialized to JSON
3. RPC message sent to Lua
4. Lua reconstructs event table
5. Event flows through three-tier dispatch chain in Lua

**Implementation:**

```js
coconut.emit = function(event) {
  // Send to Lua via RPC
  __coconut_rpc({
    type: "event",
    name: event.name,
    payload: event
  })
}
```

---

## Lifecycle events

Coconut Milk emits lifecycle events at key moments. These flow through the same three-tier dispatch chain as custom events.

### Event list

| Event | Payload | Cancellable | When fired |
|-------|---------|-------------|------------|
| `resize` | `w`, `h` | No | Window resized |
| `focus` | — | No | Window gained focus |
| `blur` | — | No | Window lost focus |
| `ready` | — | No | App initialized |
| `close` | — | Yes | App about to close |

### Emission flow

**Example: resize event**

```lua
-- C++ detects window resize
-- C++ calls Lua: coconut._dispatch("resize", { w = 1024, h = 768 }, "editor")

-- Tier 1: View handler
local editor = View.load("views/editor.html")
  :on("resize", function(event)
    reflowEditor(event.w, event.h)
  end)

-- Tier 2: Global handler
coconut.on("resize", function(event)
  updateResponsiveLayout(event.w, event.h)
end)

-- Tier 3: Fallback
function coconut.events(event)
  if event.name == "resize" then
    logResize(event.w, event.h)
  end
end
```

---

## View lifecycle events

Views have their own lifecycle events that fire when views are mounted/unmounted:

| Event | When fired |
|-------|------------|
| `mount` | View becomes active |
| `unmount` | View becomes inactive |

**Example:**

```lua
function coconut.views()
  return {
    editor = View.load("views/editor.html")
      :on_mount(function()
        print("Editor mounted")
        loadEditorState()
      end)
      :on_unmount(function()
        print("Editor unmounted")
        saveEditorState()
      end)
  }
end
```

**Note:** View lifecycle events are implemented as special cases in the view switching logic, not as regular events.

---

## Internal data structures

### coconut._listeners

Global event listeners are stored in `coconut._listeners`:

```lua
coconut._listeners = {
  save = {
    { fn = function(event) ... end, once = false },
    { fn = function(event) ... end, once = true }
  },
  resize = {
    { fn = function(event) ... end, once = false }
  }
}
```

**Structure:**
- Key: event name (string)
- Value: array of listener entries
- Each entry: `{ fn = function, once = boolean }`

---

### view._callbacks

View-scoped event handlers are stored in `view._callbacks`:

```lua
local editor = {
  kind = "file",
  value = "views/editor.html",
  _callbacks = {
    save = function(event) ... end,
    format = function(event) ... end,
    mount = function() ... end,
    unmount = function() ... end
  }
}
```

**Structure:**
- Key: event name (string)
- Value: handler function
- Includes lifecycle events (mount, unmount)

---

### coconut._view_descriptors

All registered views are stored in `coconut._view_descriptors`:

```lua
coconut._view_descriptors = {
  editor = {
    kind = "file",
    value = "views/editor.html",
    _callbacks = { ... }
  },
  settings = {
    kind = "file",
    value = "views/settings.html",
    _callbacks = { ... }
  }
}
```

**Used by:**
- View switching logic
- Event dispatch (tier 1)
- Lifecycle event emission

---

## Bridge event flow

### Lua → JavaScript (detailed)

```
1. Lua: ctx:emit({ name = "toast", message = "Saved!" })
   ↓
2. Lua: coconut._dispatch("toast", { message = "Saved!" }, "editor")
   - Tier 1: view handler (if exists)
   - Tier 2: global handlers
   - Tier 3: fallback handler
   ↓
3. C++: bridge.emit(app, "toast", { message = "Saved!" })
   - Serialize event to JSON
   ↓
4. C++: webview.eval("__coconut_dispatch_event('toast', '{\"message\":\"Saved!\"}')")
   ↓
5. JavaScript: __coconut_dispatch_event("toast", payload)
   - Parse JSON payload
   - Reconstruct event object
   ↓
6. JavaScript: coconut._dispatch("toast", event)
   - Notify all registered listeners
   - Each listener receives event object
```

---

### JavaScript → Lua (detailed)

```
1. JavaScript: coconut.emit({ name: "navigate", view: "settings" })
   ↓
2. JavaScript: __coconut_rpc({ type: "event", name: "navigate", payload: {...} })
   - Serialize to JSON
   ↓
3. C++: receive RPC message
   - Parse JSON
   - Route based on type
   ↓
4. C++: dispatchEventToLua(app, "navigate", payload)
   - Convert payload to Lua table
   ↓
5. Lua: coconut._dispatch("navigate", payload, active_view)
   - Tier 1: view handler (if exists)
   - Tier 2: global handlers
   - Tier 3: fallback handler
```

---

## Performance considerations

### Event creation overhead

Creating event objects has minimal overhead:
- Lua table allocation: ~100ns
- Metatable assignment: ~50ns
- Total: ~150ns per event

**Recommendation:** Don't worry about event creation overhead unless emitting thousands of events per frame.

---

### Listener iteration

Global listeners are iterated in order:
- Array iteration: O(n) where n = number of listeners
- Function call: ~1μs per listener

**Recommendation:** Keep listener count reasonable (<100 per event name).

---

### Propagation stopping

Stopping propagation early saves time:
- Prevents unnecessary handler calls
- Reduces memory allocations
- Improves responsiveness

**Recommendation:** Use `stopPropagation()` when event is fully handled.

---

## Debugging events

### Enable event logging

Add logging to see event flow:

```lua
-- Log all events
local original_dispatch = coconut._dispatch
coconut._dispatch = function(name, payload, target)
  print(string.format("[EVENT] %s (target: %s)", name, target or "global"))
  return original_dispatch(name, payload, target)
end
```

---

### Inspect listeners

Check registered listeners:

```lua
-- Print all global listeners
for name, listeners in pairs(coconut._listeners) do
  print(string.format("Event '%s': %d listeners", name, #listeners))
  for i, entry in ipairs(listeners) do
    print(string.format("  %d: once=%s", i, tostring(entry.once)))
  end
end

-- Print view callbacks
for view_name, view in pairs(coconut._view_descriptors) do
  print(string.format("View '%s' callbacks:", view_name))
  for event_name, _ in pairs(view._callbacks) do
    print(string.format("  - %s", event_name))
  end
end
```

---

### Trace event flow

Add trace logging to handlers:

```lua
coconut.on("save", function(event)
  print("[TRACE] Global save handler called")
  print("  defaultPrevented:", event.defaultPrevented)
  print("  propagationStopped:", event.propagationStopped)
  -- Your logic
end)
```

---

## Common patterns

### Event delegation

Use event namespacing for related events:

```lua
-- Emit namespaced events
ctx:emit({ name = "file:opened", path = "/path/to/file" })
ctx:emit({ name = "file:saved", path = "/path/to/file" })
ctx:emit({ name = "file:closed", path = "/path/to/file" })

-- Listen for specific events
coconut.on("file:opened", handler)
coconut.on("file:saved", handler)

-- Or listen for all file events
coconut.on("file:*", function(event)
  if event.name:match("^file:") then
    handleFileEvent(event)
  end
end)
```

---

### Event aggregation

Collect multiple events before processing:

```lua
local events = {}

coconut.on("data_point", function(event)
  table.insert(events, event)
  
  -- Process batch when 10 events collected
  if #events >= 10 then
    processBatch(events)
    events = {}
  end
end)
```

---

### Event transformation

Transform events before forwarding:

```lua
coconut.on("raw_data", function(event)
  -- Transform event
  local transformed = {
    name = "processed_data",
    value = event.value * 2,
    timestamp = os.time()
  }
  
  -- Emit transformed event
  ctx:emit(transformed)
end)
```

---

## Next steps

- See [How to: Handle Events](../guide/how-to-events.md) for practical patterns
- See [API Reference: coconut.on()](../reference/api-reference.md#coconuton) for full signature
- See [Bridge (Advanced)](../reference/bridge.md) for RPC protocol details
- Review [specs.md](../reference/specs/specs.md) for event model specification
