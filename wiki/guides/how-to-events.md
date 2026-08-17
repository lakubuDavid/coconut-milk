---
layout: default
title: Handle Events
parent: Guides
nav_order: 3
description: Practical patterns for working with Coconut Milk's event system.
---

# How to: Handle Events

This guide shows practical patterns for working with Coconut Milk's event system, including subscription, emission, lifecycle events, and common real-world scenarios.

---

## What you'll learn

- Subscribe to events with `coconut.on()`
- Emit events from Lua to JavaScript
- Handle lifecycle events (resize, focus, close)
- Use view-scoped events
- Control event propagation
- Implement debouncing and throttling
- Build event-driven state management

---

## Basic event patterns

### Subscribe to an event

Use `coconut.on()` to listen for events. Returns an unsubscribe function.

**Lua:**

```lua
-- Subscribe to a custom event
local unsub = coconut.on("user_logged_in", function(event)
  print("User logged in:", event.username)
  print("Role:", event.role)
end)

-- Later, unsubscribe
unsub()
```

**JavaScript:**

```js
// Subscribe to a custom event
const unsub = coconut.on("user_logged_in", (event) => {
  console.log("User logged in:", event.username)
  console.log("Role:", event.role)
})

// Later, unsubscribe
unsub()
```

---

### Emit an event

Use `ctx:emit()` (Lua) or `coconut.emit()` (JavaScript) to send events.

**Lua:**

```lua
-- Emit from a command handler
ctx:bind("login", function(params, ctx)
  local user = authenticate(params.username, params.password)
  if user then
    -- Emit event with payload
    ctx:emit({ 
      name = "user_logged_in",
      username = user.name,
      role = user.role
    })
    return { success = true }
  end
  return { success = false, error = "Invalid credentials" }
end)
```

**JavaScript:**

```js
// Emit from frontend
async function login(username, password) {
  const result = await coconut.call("login", { username, password })
  if (result.success) {
    // Event was already emitted from Lua
    showToast("Welcome back!")
  } else {
    showToast(result.error, "error")
  }
}
```

---

### One-time event listener

Use `{ once = true }` to automatically unsubscribe after the first event.

**Lua:**

```lua
coconut.on("app_initialized", function(event)
  print("App initialized, loading data...")
  loadData()
end, { once = true })
```

**JavaScript:**

```js
coconut.on("app_initialized", (event) => {
  console.log("App initialized, loading data...")
  loadData()
}, { once: true })
```

---

## Lifecycle events

Coconut Milk emits lifecycle events at key moments. Subscribe with `coconut.on()`.

### Window resize

**Lua:**

```lua
coconut.on("resize", function(event)
  print(string.format("Window resized to %dx%d", event.w, event.h))
  -- Update layout, reflow content, etc.
  updateLayout(event.w, event.h)
end)
```

**JavaScript:**

```js
coconut.on("resize", (event) => {
  console.log(`Window resized to ${event.w}x${event.h}`)
  // Update layout, reflow content, etc.
  updateLayout(event.w, event.h)
})
```

---

### Window focus/blur

**Lua:**

```lua
coconut.on("focus", function(event)
  print("Window focused")
  -- Resume animations, refresh data, etc.
  resumeAnimations()
end)

coconut.on("blur", function(event)
  print("Window blurred")
  -- Pause animations, save state, etc.
  pauseAnimations()
end)
```

**JavaScript:**

```js
coconut.on("focus", (event) => {
  console.log("Window focused")
  resumeAnimations()
})

coconut.on("blur", (event) => {
  console.log("Window blurred")
  pauseAnimations()
})
```

---

### App close (cancellable)

The `close` event can be cancelled with `preventDefault()`.

**Lua:**

```lua
coconut.on("close", function(event)
  if hasUnsavedChanges() then
    -- Cancel close
    event:preventDefault()
    -- Show save dialog
    showSaveDialog()
  end
end)
```

**JavaScript:**

```js
coconut.on("close", (event) => {
  if (hasUnsavedChanges()) {
    // Cancel close
    event.preventDefault()
    // Show save dialog
    showSaveDialog()
  }
})
```

---

## View-scoped events

Use `view:on()` to handle events only when a specific view is active.

### Editor view events

**Lua:**

```lua
function coconut.views()
  return {
    editor = View.load("views/editor.html")
      :on("save", function(event)
        -- Only fires when editor view is active
        saveFile(event.path, event.content)
        ctx:emit({ name = "save_complete" })
      end)
      :on("format", function(event)
        -- Only fires when editor view is active
        formatCode()
      end)
  }
end
```

**JavaScript:**

```js
// View-scoped events are automatically scoped
// No special JavaScript needed - just emit events
coconut.emit({ name: "save", path: currentFile, content: getContent() })
```

---

## Event propagation control

Events flow through three tiers:
1. View-scoped handlers (`view:on()`)
2. Global handlers (`coconut.on()`)
3. Fallback handler (`coconut.events()`)

### Stop propagation

Use `stopPropagation()` to prevent the event from reaching the next tier.

**Lua:**

```lua
-- View-scoped handler
local editor = View.load("views/editor.html")
  :on("save", function(event)
    saveFile()
    -- Stop propagation - global handlers won't fire
    event:stopPropagation()
  end)

-- Global handler (won't fire if view handler stops propagation)
coconut.on("save", function(event)
  print("This won't fire if view handler stops propagation")
end)
```

**JavaScript:**

```js
coconut.on("save", (event) => {
  saveFile()
  // Stop propagation - fallback handler won't fire
  event.stopPropagation()
})
```

---

### Prevent default

Use `preventDefault()` for cancellable events like `close`.

**Lua:**

```lua
coconut.on("close", function(event)
  if hasUnsavedChanges() then
    event:preventDefault()  -- Cancel the close
    promptSaveBeforeQuit()
  end
end)
```

**JavaScript:**

```js
coconut.on("close", (event) => {
  if (hasUnsavedChanges()) {
    event.preventDefault()  // Cancel the close
    promptSaveBeforeQuit()
  }
})
```

---

## Common patterns

### Debouncing resize events

Resize events fire rapidly. Debounce to avoid excessive updates.

**Lua:**

```lua
local resizeTimer = nil

coconut.on("resize", function(event)
  -- Cancel previous timer
  if resizeTimer then
    resizeTimer:stop()
  end
  
  -- Wait 150ms after last resize event
  resizeTimer = timer.setTimeout(150, function()
    updateLayout(event.w, event.h)
  end)
end)
```

**JavaScript:**

```js
let resizeTimeout = null

coconut.on("resize", (event) => {
  // Clear previous timeout
  if (resizeTimeout) {
    clearTimeout(resizeTimeout)
  }
  
  // Wait 150ms after last resize event
  resizeTimeout = setTimeout(() => {
    updateLayout(event.w, event.h)
  }, 150)
})
```

---

### Throttling scroll events

Throttle to process events at a fixed interval.

**JavaScript:**

```js
let lastScrollTime = 0
const throttleDelay = 100  // ms

coconut.on("scroll", (event) => {
  const now = Date.now()
  if (now - lastScrollTime >= throttleDelay) {
    lastScrollTime = now
    updateScrollIndicator(event.scrollTop)
  }
})
```

---

### Event-driven state management

Use events to synchronize state between Lua and JavaScript.

**Lua:**

```lua
-- Application state
local state = {
  theme = "dark",
  fontSize = 14,
  sidebarOpen = true
}

-- Command to update state
ctx:bind("update_settings", function(params, ctx)
  -- Update state
  if params.theme then state.theme = params.theme end
  if params.fontSize then state.fontSize = params.fontSize end
  if params.sidebarOpen ~= nil then state.sidebarOpen = params.sidebarOpen end
  
  -- Emit event to notify all listeners
  ctx:emit({ 
    name = "settings_changed",
    theme = state.theme,
    fontSize = state.fontSize,
    sidebarOpen = state.sidebarOpen
  })
  
  return { success = true }
end)

-- Listen for settings changes (e.g., to persist)
coconut.on("settings_changed", function(event)
  saveSettings(event)
end)
```

**JavaScript:**

```js
await coconut.ready()

// Listen for settings changes
coconut.on("settings_changed", (event) => {
  // Update UI
  document.body.className = event.theme
  document.body.style.fontSize = `${event.fontSize}px`
  
  const sidebar = document.getElementById("sidebar")
  sidebar.style.display = event.sidebarOpen ? "block" : "none"
})

// Update settings
async function toggleSidebar() {
  await coconut.call("update_settings", { 
    sidebarOpen: !currentSidebarState 
  })
}
```

---

### Cross-view communication

Use events to communicate between views without direct coupling.

**Lua:**

```lua
-- File explorer view
local fileExplorer = View.load("views/file-explorer.html")
  :on_mount(function()
    -- Listen for file open requests from other views
    coconut.on("open_file", function(event)
      openFile(event.path)
    end)
  end)

-- Editor view
local editor = View.load("views/editor.html")
  :on("file_selected", function(event)
    -- Emit event for file explorer to handle
    ctx:emit({ 
      name = "open_file",
      path = event.path
    })
  end)
```

**JavaScript:**

```js
// In editor view
document.getElementById("fileList").addEventListener("click", (e) => {
  if (e.target.dataset.path) {
    coconut.emit({ 
      name: "file_selected",
      path: e.target.dataset.path
    })
  }
})

// In file explorer view
coconut.on("open_file", (event) => {
  loadFile(event.path)
})
```

---

### Error handling in event handlers

Wrap event handlers in try-catch to prevent one error from breaking the chain.

**Lua:**

```lua
coconut.on("data_received", function(event)
  local ok, err = pcall(function()
    processData(event.data)
  end)
  
  if not ok then
    print("Error in event handler:", err)
    -- Emit error event for UI to handle
    ctx:emit({ 
      name = "error",
      message = "Failed to process data",
      details = err
    })
  end
end)
```

**JavaScript:**

```js
coconut.on("data_received", (event) => {
  try {
    processData(event.data)
  } catch (error) {
    console.error("Error in event handler:", error)
    // Emit error event for UI to handle
    coconut.emit({ 
      name: "error",
      message: "Failed to process data",
      details: error.message
    })
  }
})
```

---

### Cleanup on view unmount

Unsubscribe from events when a view unmounts to prevent memory leaks.

**Lua:**

```lua
local unsubscribers = {}

function coconut.views()
  return {
    dashboard = View.load("views/dashboard.html")
      :on_mount(function()
        -- Subscribe to events
        table.insert(unsubscribers, coconut.on("data_update", function(event)
          updateDashboard(event)
        end))
        
        table.insert(unsubscribers, coconut.on("settings_changed", function(event)
          refreshDashboard(event)
        end))
      end)
      :on_unmount(function()
        -- Unsubscribe from all events
        for _, unsub in ipairs(unsubscribers) do
          unsub()
        end
        unsubscribers = {}
      end)
  }
end
```

**JavaScript:**

```js
const unsubscribers = []

coconut.on("view:dashboard:mount", () => {
  // Subscribe to events
  unsubscribers.push(
    coconut.on("data_update", (event) => {
      updateDashboard(event)
    })
  )
  
  unsubscribers.push(
    coconut.on("settings_changed", (event) => {
      refreshDashboard(event)
    })
  )
})

coconut.on("view:dashboard:unmount", () => {
  // Unsubscribe from all events
  unsubscribers.forEach(unsub => unsub())
  unsubscribers.length = 0
})
```

---

## Event namespaces

Use namespaced event names to organize events by feature or module.

**Lua:**

```lua
-- File operations
ctx:emit({ name = "file:opened", path = "/path/to/file" })
ctx:emit({ name = "file:saved", path = "/path/to/file" })
ctx:emit({ name = "file:closed", path = "/path/to/file" })

-- User operations
ctx:emit({ name = "user:login", username = "alice" })
ctx:emit({ name = "user:logout" })

-- Editor operations
ctx:emit({ name = "editor:format", success = true })
ctx:emit({ name = "editor:lint", errors = {} })
```

**JavaScript:**

```js
// Listen for namespaced events
coconut.on("file:opened", (event) => {
  console.log("File opened:", event.path)
})

coconut.on("user:login", (event) => {
  console.log("User logged in:", event.username)
})
```

---

## Best practices

1. **Always unsubscribe when done** - Prevent memory leaks by calling the unsubscribe function
2. **Use namespaced events** - Organize events with colons (e.g., `file:opened`, `user:login`)
3. **Keep event payloads flat** - Avoid deeply nested objects for easier access
4. **Handle errors in event handlers** - Wrap handlers in try-catch to prevent breaking the chain
5. **Debounce high-frequency events** - Resize, scroll, and mouse events fire rapidly
6. **Use view-scoped events for view-specific logic** - Avoids conflicts between views
7. **Emit events for cross-view communication** - Decouples views from each other
8. **Use `{ once = true }` for one-time events** - Automatically unsubscribes after first event

---

## Next steps

- See [API Reference: coconut.on()](../reference/api-reference.md#coconuton) for full signature
- See [API Reference: ctx:emit()](../reference/api-reference.md#ctxemitevent) for emission details
- Learn about [Using Keybinds](./how-to-keybinds.md) for keyboard event handling
- Check [Event Model](../explanation/concepts.md#event-model) for architectural details
