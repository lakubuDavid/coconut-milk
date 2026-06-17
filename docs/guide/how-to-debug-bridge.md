# How to: Debug Bridge Issues

This guide helps you diagnose and fix common problems with Coconut Milk's bridge communication between Lua and JavaScript.

---

## What you'll learn

- Enable debug logging and transport dumps
- Diagnose command call failures
- Debug event delivery issues
- Fix serialization problems
- Use browser DevTools for bridge inspection
- Handle common error codes

---

## Enable debug logging

### Basic debug mode

Run your app with the `--debug` flag to see runtime logs:

```bash
coconut --debug
```

This enables:
- Startup logs (config, views, commands)
- Bridge lifecycle events
- Error messages and warnings

---

### Transport dump (verbose)

To see all RPC messages flowing through the bridge, enable transport dump in your config:

**coconut.config.lua:**

```lua
return {
  debug = {
    enabled = true,
    showTransportDump = true,  -- Log all RPC messages
    logLevel = "debug"
  }
}
```

**Output example:**

```
[DEBUG] transport: send → {"type":"call","id":"u1","name":"greet","payload":{"name":"Ada"}}
[DEBUG] transport: recv → {"type":"return","id":"u1","payload":{"greeting":"Hello, Ada!"}}
[DEBUG] transport: send → {"type":"event","name":"toast","payload":{"message":"Saved!"}}
```

**Log levels:**
- `debug` — All messages including `[DEBUG]`
- `info` — `[INFO]`, `[WARN]`, `[ERROR]` (default)
- `warn` — `[WARN]`, `[ERROR]`
- `error` — Only `[ERROR]`

---

## Diagnose command call failures

### Command not found

**Error:** `CommandNotFound: No handler for 'command_name'`

**Causes:**
- Command not registered with `ctx:bind()`
- Command file not in `commands/` directory
- `coconut generate` not run after adding `@command` annotation

**Debug steps:**

1. Check if command is registered:

```lua
function coconut.commands(ctx)
  ctx:bind("my_command", function(params, ctx)
    print("Command called!")
    return { success = true }
  end)
  
  -- List all registered commands
  for name, _ in pairs(ctx.commands.handlers) do
    print("Registered command:", name)
  end
end
```

2. Verify `@command` annotation in command file:

```lua
-- commands/my_command.lua
---@command my_command
---@param params { value: string }
---@return { success: boolean }
local function my_command(params, ctx)
  return { success = true }
end

return { my_command = my_command }
```

3. Run code generation:

```bash
coconut generate
```

4. Check generated files exist:

```bash
ls generated/my_command.g.lua
```

---

### Command returns wrong type

**Error:** JavaScript receives unexpected data structure

**Causes:**
- Lua returns non-table value
- Serialization mismatch (arrays vs objects)

**Debug steps:**

1. Check Lua return value:

```lua
ctx:bind("get_data", function(params, ctx)
  local result = { items = {"a", "b", "c"} }
  
  -- Debug: print what we're returning
  print("Returning:", coconut.json.stringify(result))
  
  return result
end)
```

2. Verify array vs object serialization:

```lua
-- Sequential 1-indexed table → JSON array
return { "a", "b", "c" }  -- ["a", "b", "c"]

-- Non-sequential or string keys → JSON object
return { x = 1, y = 2 }  -- {"x": 1, "y": 2}

-- Mixed → JSON object (with numeric string keys)
return { "a", x = 1 }    -- {"1": "a", "x": 1}
```

3. Check JavaScript side:

```js
try {
  const result = await coconut.call("get_data")
  console.log("Received:", result)
  console.log("Type:", typeof result)
  console.log("Is array:", Array.isArray(result))
} catch (error) {
  console.error("Error:", error.code, error.message)
}
```

---

### Command throws Lua error

**Error:** `LuaError: [string "commands/my_command.lua"]:10: attempt to index a nil value`

**Causes:**
- Lua runtime error in command handler
- Missing nil check
- Invalid table access

**Debug steps:**

1. Wrap command in pcall:

```lua
ctx:bind("risky_command", function(params, ctx)
  local ok, result = pcall(function()
    -- Your code here
    return doSomethingRisky(params.value)
  end)
  
  if not ok then
    print("Command failed:", result)
    return { error = result }
  end
  
  return { success = true, data = result }
end)
```

2. Add nil checks:

```lua
ctx:bind("process", function(params, ctx)
  if not params then
    return { error = "params is nil" }
  end
  
  if not params.data then
    return { error = "params.data is nil" }
  end
  
  -- Safe to use params.data
  return processData(params.data)
end)
```

3. Check error details in JavaScript:

```js
try {
  await coconut.call("risky_command", { value: 123 })
} catch (error) {
  console.error("Code:", error.code)
  console.error("Message:", error.message)
  console.error("Details:", error.details)  // Lua stack trace
}
```

---

## Debug event delivery issues

### Event not received

**Problem:** JavaScript listener doesn't fire when Lua emits event

**Debug steps:**

1. Verify event is emitted:

```lua
ctx:bind("trigger", function(params, ctx)
  print("Emitting event...")
  ctx:emit({ name = "my_event", data = "test" })
  print("Event emitted")
  return { success = true }
end)
```

2. Check JavaScript listener registration:

```js
await coconut.ready()

console.log("Registering listener...")
const unsub = coconut.on("my_event", (event) => {
  console.log("Event received:", event)
})
console.log("Listener registered")
```

3. Enable transport dump to see event message:

```lua
-- coconut.config.lua
return {
  debug = {
    showTransportDump = true
  }
}
```

Expected output:

```
[DEBUG] transport: send → {"type":"event","name":"my_event","payload":{"data":"test"}}
```

4. Check event name spelling (case-sensitive):

```lua
-- Wrong
ctx:emit({ name = "MyEvent" })

-- Correct
ctx:emit({ name = "my_event" })
```

---

### Event received but payload missing

**Problem:** Event fires but payload fields are missing or nil

**Causes:**
- Payload not included in emit
- Field name typo
- Nil values not serialized

**Debug steps:**

1. Check emit payload:

```lua
ctx:emit({ 
  name = "user_login",
  username = "alice",
  role = "admin"
})

-- Debug: print what we're sending
local payload = { 
  name = "user_login",
  username = "alice",
  role = "admin"
}
print("Payload:", coconut.json.stringify(payload))
ctx:emit(payload)
```

2. Check JavaScript event object:

```js
coconut.on("user_login", (event) => {
  console.log("Full event:", event)
  console.log("Keys:", Object.keys(event))
  console.log("username:", event.username)
  console.log("role:", event.role)
})
```

3. Remember: nil values are not serialized

```lua
-- Wrong: role is nil, won't be sent
ctx:emit({ 
  name = "user_login",
  username = "alice",
  role = nil  -- This field won't appear in JavaScript
})

-- Correct: use false or empty string instead
ctx:emit({ 
  name = "user_login",
  username = "alice",
  role = ""  -- or false, or "guest"
})
```

---

### Lifecycle event not firing

**Problem:** `resize`, `focus`, `blur`, or `ready` event doesn't fire

**Debug steps:**

1. Check listener registration timing:

```js
// Wrong: register before ready
coconut.on("resize", (event) => {
  console.log("Resize:", event.w, event.h)
})
await coconut.ready()

// Correct: register after ready
await coconut.ready()
coconut.on("resize", (event) => {
  console.log("Resize:", event.w, event.h)
})
```

2. Verify event name (lowercase):

```js
// Wrong
coconut.on("Resize", handler)
coconut.on("RESIZE", handler)

// Correct
coconut.on("resize", handler)
```

3. Test with simple handler:

```js
await coconut.ready()

coconut.on("resize", (event) => {
  console.log("Resize event fired!")
  console.log("Width:", event.w)
  console.log("Height:", event.h)
})

// Manually resize window to trigger event
coconut.call("set_window_size", { w: 1000, h: 800 })
```

---

## Fix serialization problems

### Circular reference error

**Error:** `JSON.stringify cannot serialize cyclic structures`

**Cause:** Lua table contains circular references

**Debug steps:**

1. Check for circular references:

```lua
-- Wrong: circular reference
local data = {}
data.self = data
ctx:emit({ name = "data", payload = data })  -- Error!

-- Correct: avoid circular references
local data = { id = 1, name = "test" }
ctx:emit({ name = "data", payload = data })
```

2. Use custom serializer to detect cycles:

```lua
local function safeStringify(obj, seen)
  seen = seen or {}
  
  if type(obj) ~= "table" then
    return tostring(obj)
  end
  
  if seen[obj] then
    return "[circular]"
  end
  
  seen[obj] = true
  
  local parts = {}
  for k, v in pairs(obj) do
    table.insert(parts, k .. "=" .. safeStringify(v, seen))
  end
  
  return "{" .. table.concat(parts, ", ") .. "}"
end

print("Data:", safeStringify(data))
```

---

### UTF-8 encoding issues

**Problem:** Special characters appear corrupted

**Causes:**
- Invalid UTF-8 bytes in string
- Null bytes in string

**Debug steps:**

1. Check string encoding:

```lua
local text = "Hello 世界 🌍"
print("Length:", #text)
print("Bytes:", text:byte(1, -1))

-- Sanitize before sending
local clean = text:gsub("[\x00-\x1F\x7F]", "")  -- Remove control chars
ctx:emit({ name = "message", text = clean })
```

2. Verify JavaScript receives correct encoding:

```js
coconut.on("message", (event) => {
  console.log("Text:", event.text)
  console.log("Length:", event.text.length)
  console.log("Code points:", [...event.text].map(c => c.codePointAt(0)))
})
```

---

## Use browser DevTools

### Open DevTools

**macOS:** Press `Cmd+Option+I` or run:

```lua
ctx:bind("open_devtools", function(params, ctx)
  -- macOS: open WebKit inspector
  os.execute("open -a 'Safari' --args -webinspector")
  return { success = true }
end)
```

**Windows/Linux:** Use debug build with inspector enabled

---

### Inspect bridge messages

1. Open Console tab
2. Filter by `coconut` to see bridge logs
3. Check Network tab for any failed requests

**Console commands:**

```js
// Check if coconut is available
console.log(window.coconut)

// List registered event listeners
console.log(coconut._listeners)

// Manually emit event
coconut.emit({ name: "test_event", data: "debug" })

// Call command
coconut.call("ping").then(r => console.log(r))
```

---

### Debug event flow

Add logging to event handlers:

```js
coconut.on("my_event", (event) => {
  console.group("my_event received")
  console.log("Timestamp:", new Date().toISOString())
  console.log("Event object:", event)
  console.log("Event name:", event.name)
  console.log("Event payload:", event.payload)
  console.trace("Stack trace")
  console.groupEnd()
  
  // Your handler logic
  processEvent(event)
})
```

---

## Common error codes

### CommandNotFound

**Cause:** Command not registered

**Fix:**
- Verify `ctx:bind()` is called
- Run `coconut generate` for annotated commands
- Check command name spelling

---

### LuaError

**Cause:** Lua runtime error in command handler

**Fix:**
- Wrap handler in `pcall`
- Add nil checks
- Check error details for stack trace

---

### NotReady

**Cause:** Bridge not ready when call made

**Fix:**
- Wait for `coconut.ready()` before calling commands
- Use `await coconut.ready()` at startup

---

### QueueOverflow

**Cause:** Too many events queued before bridge ready

**Fix:**
- Reduce event emission during startup
- Increase queue size (advanced)
- Ensure bridge becomes ready quickly

---

### BridgeError

**Cause:** Malformed RPC message or protocol error

**Fix:**
- Check message format matches RPC envelope spec
- Verify JSON serialization
- Enable transport dump to inspect messages

---

## Debugging checklist

When bridge communication fails, check:

- [ ] Debug mode enabled (`--debug` flag)
- [ ] Transport dump enabled (shows all messages)
- [ ] Command is registered (check logs)
- [ ] Event name matches exactly (case-sensitive)
- [ ] Payload is valid (no circular refs, no nil)
- [ ] Bridge is ready (`await coconut.ready()`)
- [ ] Listener is registered before event fires
- [ ] No Lua errors in command handler
- [ ] JSON serialization works (test with `coconut.json.stringify`)
- [ ] Browser DevTools shows no JavaScript errors

---

## Next steps

- See [Bridge (Advanced)](../reference/bridge.md) for protocol details
- See [API Reference: Error Codes](../reference/api-reference.md#error-codes) for full error list
- Check [Troubleshooting](../explanation/troubleshooting.md) for common issues
- Review [Event Handling Patterns](./how-to-events.md) for event best practices
