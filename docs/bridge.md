# Bridge (Advanced)

The Coconut Milk bridge is the communication layer between the **frontend (JavaScript)** and the **backend (Lua)**. Understanding how it works is essential for debugging, extending, and optimizing your app.

---

## RPC Protocol

All bridge traffic uses a canonical RPC envelope with five message types:

### Message Types

| Type | Direction | Purpose |
|---|---|---|
| `call` | Frontend → Lua | Invoke a registered command |
| `return` | Lua → Frontend | Successful response to a call |
| `error` | Lua → Frontend | Error response to a call |
| `event` | Either direction | Fire-and-forget message |
| `ready` | Frontend → Runtime | Bridge initialization handshake |

### JSON Envelope Shape

```json
// Call: Frontend invokes a Lua command
{ "type": "call",   "id": "uuid",  "name": "greet", "payload": { "name": "Ada" } }

// Return: Lua command succeeds
{ "type": "return", "id": "uuid",  "payload": { "greeting": "Hello, Ada!" } }

// Error: Lua command fails
{ "type": "error",  "id": "uuid",  "payload": { "code": "CommandNotFound", "message": "No handler" } }

// Event: One-way message (either direction)
{ "type": "event",  "name": "toast", "payload": { "message": "Saved!" } }

// Ready: Frontend signals bridge is initialized
{ "type": "ready" }
```

### Envelope Rules

| Field | Required For | Purpose |
|---|---|---|
| `type` | All messages | Discriminator — determines message handling |
| `id` | `call`, `return`, `error` | Correlation ID for Promise tracking |
| `name` | `call`, `event` | Command name or event name |
| `payload` | `call`, `return`, `event` | Data (object, array, or primitive) |
| (none) | `ready` | No additional fields |

---

## Message Flow

### coconut.call() Path

```
1. Frontend JS:
   await coconut.call("greet", { name: "Ada" })

2. JS serializes payload:
   JSON.stringify({ name: "Ada" }) → '{"name":"Ada"}'

3. JS sends the RPC envelope to the runtime

4. The runtime dispatches to the Lua handler:
   → params table = { name = "Ada" }

5. Lua command runs:
   return { greeting = "Hello, " .. name .. "!" }

6. The runtime serializes the result as JSON:
   { "type": "return", "id": "uuid", "payload": { "greeting": "Hello, Ada!" } }

7. JS receives the response:
   Promise resolves → { greeting: "Hello, Ada!" }
```

### ctx:emit() Path

```
1. Lua backend:
   ctx:emit("toast", { message = "Saved!", type = "success" })

2. The runtime serializes the event envelope and sends it to the webview

3. Injected JS runtime parses the payload:
   JSON.parse('{"message":"Saved!","type":"success"}')

4. JS invokes registered listeners:
   coconut.on("toast", callback) → callback({ message: "Saved!", type: "success" })
```

---

## Transport Layer

The transport layer handles the **actual delivery** of RPC messages between the runtime and the webview.

### Outbound (Runtime → JS)

Messages are delivered to the frontend by evaluating JavaScript in the webview context. The injected JS function `__coconut_rpc_receive` parses the JSON and dispatches:

- `return` → resolves the corresponding Promise
- `error` → rejects the corresponding Promise
- `event` → invokes registered listeners

### Inbound (JS → Runtime)

When the frontend calls a function like `__coconut_rpc(jsonString)`, the runtime receives the JSON string, parses it, and dispatches based on the `type` field:

- `call` → look up the command handler, invoke it, send response
- `event` → dispatch to the Lua `coconut.events()` handler
- `ready` → mark the bridge as ready, deliver queued events

### Script Injection

At webview creation, Coconut injects a JavaScript runtime that provides the `coconut` global API:

```javascript
window.coconut = {
  ready: function() { ... },
  call: function(name, payload) { ... },
  emit: function(name, payload) { ... },
  on: function(name, fn) { ... },
  // ... helpers
}
```

This injection happens before any page scripts run, so `coconut` is available immediately.

---

## Readiness Handshake

The bridge uses a **two-way handshake** to determine when it's ready for communication.

### Handshake Sequence

```
1. Runtime initializes the bridge and loads the frontend HTML
2. Frontend JS initializes (DOM ready, coconut object available)
3. Frontend sends: { "type": "ready" }
4. Runtime acknowledges — bridge is now active
5. Queued events are delivered, waiting calls proceed
```

### Pre-Ready Behavior

| Operation | Behavior |
|---|---|
| `coconut.call()` | **Waits** — Promise resolves after readiness |
| `coconut.emit()` | **Queues** — Event delivered after readiness |
| `coconut.on()` | **Registers immediately** — Listener fires after readiness |
| `ctx:emit()` | **Queues** — Event delivered after readiness |

### Queue Limits

The event queue has a finite capacity. If it overflows before readiness:

```lua
-- Lua side:
ctx:emit("event", { ... })  -- 100th event while not ready
-- → Returns false, logs "QueueOverflow" error
```

```js
// Frontend side:
await coconut.emit("event", { ... })  // While not ready
// → Promise rejects with { code: "QueueOverflow", message: "..." }
```

### How coconut.ready() Works

```javascript
window.coconut.ready = function() {
  return new Promise((resolve) => {
    if (bridgeReady) {
      resolve();
    } else {
      readyCallbacks.push(resolve);
    }
  });
}
```

When the bridge initializes, `__coconut_bridge_ready()` is called:

```javascript
window.__coconut_bridge_ready = function() {
  bridgeReady = true;
  for (const cb of readyCallbacks) cb();
  readyCallbacks = [];
  flushQueuedEvents();
  flushQueuedCalls();
};
```

---

## Serialization

### JSON Encoding

All payloads are serialized to JSON:

| Lua Type | JSON Type | Example |
|---|---|---|
| `table` (sequential) | `array` | `{1, 2, 3}` → `[1, 2, 3]` |
| `table` (key-value) | `object` | `{name = "Ada"}` → `{"name": "Ada"}` |
| `string` | `string` | `"Hello"` → `"Hello"` |
| `number` | `number` | `42` → `42` |
| `boolean` | `boolean` | `true` → `true` |
| `nil` | `null` | `nil` → `null` |

### Array Detection

The Lua-to-JSON converter uses the **maxIndex >= count heuristic**:

```lua
-- Sequential 1-indexed table → JSON array
local arr = { "a", "b", "c" }
-- maxIndex = 3, count = 3 → array: ["a", "b", "c"]

-- Non-sequential table → JSON object
local obj = { name = "Ada", age = 30 }
-- No numeric keys → object: {"name": "Ada", "age": 30}

-- Mixed table → JSON object
local mixed = { "a", name = "Ada" }
-- Has string keys → object: {"1": "a", "name": "Ada"}
```

### UTF-8 Handling

Invalid UTF-8 bytes in strings are replaced with the Unicode replacement character (U+FFFD) during serialization. Null bytes (`0x00`) are also removed — they can't be represented in JSON.

---

## Frontend Helpers

The injected `coconut` global provides several helper methods.

### `coconut.ready()`

```typescript
await coconut.ready(): Promise<void>
```

Returns a Promise that resolves when the bridge is ready. **Always call this before any other coconut method.**

```js
await coconut.ready()
// Now safe to call coconut.call(), coconut.emit(), etc.
```

### `coconut.call()`

```typescript
await coconut.call<TResponse, TPayload>(
  name: TCommandName,
  payload?: TPayload,
): Promise<TResponse>
```

Calls a Lua command and returns the result.

```js
const result = await coconut.call("greet", { name: "Ada" })
// result: { greeting: string }
```

**Error handling:**

```js
try {
  const result = await coconut.call("greet", { name: "Ada" })
  console.log(result.greeting)
} catch (err) {
  console.error(`Command failed: ${err.code} — ${err.message}`)
}
```

### `coconut.emit()`

```typescript
await coconut.emit(name: string, payload?: object): Promise<void>
```

Emits an event to the Lua backend.

```js
await coconut.emit("navigate", { view: "settings" })
```

### `coconut.on()`

```typescript
coconut.on(name: string, fn: (payload: object) => void): () => void
```

Registers a listener for Lua-emitted events. Returns an unsubscribe function.

```js
const unsub = coconut.on("toast", (payload) => {
  console.log(payload.message)
})

// Later:
unsub()  // Stop listening
```

### `coconut.views()`

```typescript
await coconut.views(): Promise<string[]>
```

Returns the list of registered view names.

```js
const views = await coconut.views()
// ["home", "settings", "about"]
```

### `coconut.ping()`

```typescript
await coconut.ping(): Promise<string>
```

Connectivity test. Returns `"pong"`.

```js
const pong = await coconut.ping()
// "pong"
```

### `coconut.window`

```typescript
coconut.window: {
  minimize(): Promise<void>
  toggleFullscreen(): Promise<void>
  close(): Promise<void>
}
```

Window control helpers.

```js
await coconut.window.minimize()
await coconut.window.toggleFullscreen()
await coconut.window.close()
```

### `coconut.fs`

```typescript
coconut.fs: {
  readText(path: string): Promise<{ ok: boolean; data?: string; error?: string }>
}
```

Filesystem helpers.

```js
const { ok, data, error } = await coconut.fs.readText("/path/to/file.txt")
if (ok) console.log(data)
else console.error(error)
```

---

## Error Handling

### Error Codes

| Code | Description | Typical Cause |
|---|---|---|
| `Ok` | Success | — |
| `Unknown` | Unknown error | Uncategorized failure |
| `InvalidConfig` | Config parse error | Malformed `coconut.config.lua` |
| `InvalidView` | Invalid view descriptor | Unknown view kind |
| `MissingFile` | File not found | View or asset doesn't exist |
| `DuplicateCommand` | Command already registered | Two `ctx:bind()` with same name |
| `CommandNotFound` | Command not found | `coconut.call()` with unregistered name |
| `InvalidPayload` | Invalid payload format | Non-table payload |
| `NotReady` | Bridge not ready | Called before `coconut.ready()` |
| `QueueOverflow` | Event queue overflow | Too many events before readiness |
| `LuaError` | Lua runtime error | Exception in command handler |
| `BridgeError` | Bridge protocol error | Malformed RPC envelope |
| `WebViewError` | Webview error | Webview failure |
| `ParseError` | Parse error | Invalid JSON in message |

### Error Envelope

Error responses use the `error` type:

```json
{
  "type": "error",
  "id": "uuid",
  "payload": {
    "code": "CommandNotFound",
    "message": "No handler for 'unknown'"
  }
}
```

### Frontend Error Handling

```js
try {
  const result = await coconut.call("unknown", {})
} catch (err) {
  // err is a CoconutError:
  // { code: "CommandNotFound", message: "No handler for 'unknown'" }
  console.error(`${err.code}: ${err.message}`)
}
```

### Lua Error Handling

Commands should use `pcall` for safety:

```lua
local function risky_operation(params, ctx)
  local ok, result = pcall(function()
    return coconut.fs.readText(params.path)
  end)

  if not ok then
    return { error = tostring(result) }
  end

  return { data = result }
end
```

### Dialog Exception Safety

Dialog calls (open, save, messageBox) are handled safely by the runtime — exceptions are caught and logged instead of crashing the app. In Lua, always wrap dialog calls in `pcall` for an extra safety layer:

```lua
local ok, result = pcall(coconut.dialog.open, "Open File", false, true)
if not ok then
  coconut.error("Dialog failed: " .. tostring(result))
  return { cancelled = true }
end
```

---

## TypeScript Definitions

### coconut.d.ts

The main type definition file provides ambient typings for `window.coconut`:

```typescript
export {};

/// <reference path="./generated/commands.d.ts" />

export interface CoconutError {
  code: string;
  message: string;
  details?: unknown;
}

export interface CoconutJsAPI<TCommandName extends string = CoconutCommandName> {
  ready(): Promise<void>;
  call<TResponse = unknown, TPayload = Record<string, unknown>>(
    name: TCommandName,
    payload?: TPayload,
  ): Promise<TResponse>;
  emit<TPayload = Record<string, unknown>>(
    name: string,
    payload?: TPayload,
  ): Promise<void>;
  on<TPayload = Record<string, unknown>>(
    name: string,
    fn: (payload: TPayload) => void,
  ): () => void;
  views(): Promise<string[]>;
  ping(): Promise<string>;
  window: CoconutWindowAPI;
  fs: CoconutFsAPI;
}

declare global {
  interface Window {
    coconut: CoconutJsAPI;
  }
}
```

**Key design:**

- `export {}` at the top makes the file a module, allowing `declare global`
- `/// <reference path="./generated/commands.d.ts" />` imports generated types
- `CoconutCommandName` as default generic enables typed `coconut.call()`

### generated/commands.d.ts

Generated by `coconut generate` from `@command` annotations:

```typescript
// generated/commands.d.ts (auto-generated)
export type CoconutCommandName = "greet" | "farewell" | "ping" | "save_settings";
```

This union type is used as the default for `TCommandName` in `coconut.call()`:

```typescript
await coconut.call("greet", { name: "Ada" })  // ✅ Type-checked
```

### tsconfig.json Setup

For JS type-checking:

```json
{
  "compilerOptions": {
    "target": "ES2022",
    "module": "NodeNext",
    "moduleResolution": "NodeNext",
    "strict": true,
    "checkJs": true,
    "allowJs": true
  },
  "include": ["views/**/*", "assets/**/*", "coconut.d.ts"]
}
```

For TypeScript:

```json
{
  "compilerOptions": {
    "target": "ES2022",
    "module": "NodeNext",
    "moduleResolution": "NodeNext",
    "strict": true,
    "outDir": "dist",
    "rootDir": "src"
  },
  "include": ["src/**/*", "coconut.d.ts"]
}
```

---

## Next Steps

- Check the **[API Reference](./api-reference.md)** for all functions
- See **[Examples](./examples.md)** for real-world patterns
- Read **[Troubleshooting](./troubleshooting.md)** for common issues
