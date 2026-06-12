# Bridge (Advanced)

The Coconut Milk bridge is the communication layer between the **frontend (JavaScript)** and the **backend (Lua)**. Understanding how it works is essential for debugging, extending, and optimizing your app.

---

## RPC Protocol

All bridge traffic uses a **canonical RPC envelope** defined in `src/rpc_envelope.h`:

### Message Types

```cpp
enum class Type {
  kCall,    // Request a command call (JS → Lua)
  kReturn,  // Successful response (Lua → JS)
  kError,   // Error response (Lua → JS)
  kEvent,   // Fire-and-forget event (either direction)
  kReady,   // Bridge readiness handshake (JS → C++)
};
```

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

![RPC Call Flow](./diagrams/rpc-call.png)

```
1. Frontend JS:
   await coconut.call("greet", { name: "Ada" })

2. JS serializes payload:
   JSON.stringify({ name: "Ada" }) → '{"name":"Ada"}'

3. JS sends RPC envelope via webview_bind:
   __coconut_call("greet", '{"name":"Ada"}')

4. C++ receives via static_on_rpc callback:
   Parse JSON → extract {"type":"call", "id":"u1", "name":"greet", "payload":{...}}

5. C++ dispatches to Lua:
   sol2: fn(paramsTable, context)
   → paramsTable = { name = "Ada" }

6. Lua command runs:
   return { greeting = "Hello, " .. name .. "!" }

7. C++ serializes result:
   { "type": "return", "id": "u1", "payload": { "greeting": "Hello, Ada!" } }

8. C++ sends response via webview_eval:
   __coconut_rpc_receive('{"type":"return","id":"u1","payload":{...}}')

9. JS parses and resolves Promise:
   Promise resolves → { greeting: "Hello, Ada!" }
```

### ctx:emit() Path

![Event Flow](./diagrams/event-flow.png)

```
1. Lua backend:
   ctx:emit("toast", { message = "Saved!", type = "success" })

2. C++ serializes event envelope:
   { "type": "event", "name": "toast", "payload": { "message": "Saved!", "type": "success" } }

3. C++ sends via webview_eval:
   __coconut_dispatch_event("toast", '{"message":"Saved!","type":"success"}')

4. Injected JS runtime parses payload:
   JSON.parse('{"message":"Saved!","type":"success"}')

5. JS invokes registered listeners:
   coconut.on("toast", callback) → callback({ message: "Saved!", type: "success" })
```

---

## Transport Layer

The transport layer handles the **actual delivery** of RPC messages between C++ and the webview.

### Outbound (C++ → JS)

Messages are sent via `webview_eval()`:

```cpp
// In webview_transport.cpp
void WebviewTransport::send(const rpc::Message& msg) {
  std::string json = msg.toJson().dump();
  // Inject into webview as JavaScript
  webview_eval(m_webview,
    ("__coconut_rpc_receive('" + json + "')").c_str());
}
```

The injected JavaScript (`__coconut_rpc_receive`) parses the JSON and dispatches:

- `return` → resolves the corresponding Promise
- `error` → rejects the corresponding Promise
- `event` → invokes registered listeners

### Inbound (JS → C++)

Messages are received via `webview_bind()`:

```cpp
// In webview_transport.cpp
webview_bind(m_webview, "__coconut_rpc", &WebviewTransport::static_on_rpc, this);
```

When the frontend calls `__coconut_rpc(jsonString)`, the C++ callback fires:

```cpp
void WebviewTransport::static_on_rpc(const char* id, const char* req, void* arg) {
  // req = JSON.stringify([msgJson]) — array-wrapped for webview_bind compatibility
  auto args = nlohmann::json::parse(req);
  auto msgJson = args[0].get<std::string>();
  auto msg = rpc::Message::fromJson(msgJson);
  // Dispatch based on msg.type
}
```

### Script Injection

At webview creation, Coconut injects a JavaScript runtime that provides the `coconut` global API:

```javascript
// Injected via WKUserScript (document_start)
window.coconut = {
  ready: function() { ... },
  call: function(name, payload) { ... },
  emit: function(name, payload) { ... },
  on: function(name, fn) { ... },
  // ... helpers
}
```

This injection happens at `WKUserScriptInjectionTimeAtDocumentStart`, so `coconut` is available before any page scripts run.

---

## Readiness Handshake

The bridge uses a **two-way handshake** to determine when it's ready for communication.

### Handshake Sequence

```
1. C++ creates webview, loads HTML, injects coconut JS runtime
2. Frontend JS initializes (DOM ready, coconut object available)
3. Frontend sends: { "type": "ready" }
4. C++ acknowledges — bridge is now active
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

C++ calls `__coconut_bridge_ready()` when the bridge is initialized:

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

All payloads are serialized to JSON using `nlohmann::json`:

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

`nlohmann::json::dump()` throws on invalid UTF-8 by default. Coconut uses `error_handler_t::replace` to substitute invalid sequences:

```cpp
json.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
// Invalid UTF-8 bytes are replaced with U+FFFD (replacement character)
```

### String Sanitization

The `sanitizeJsonString()` function in `src/bridge.cpp` removes:

- **Null bytes** (`0x00`) — Can terminate strings prematurely
- **Invalid UTF-8 sequences** — Multi-byte sequences that don't form valid characters

---

## Frontend Helpers

The injected `coconut` global provides several helper methods:

### coconut.ready()

```typescript
await coconut.ready(): Promise<void>
```

Returns a Promise that resolves when the bridge is ready. **Always call this before any other coconut method.**

```js
await coconut.ready()
// Now safe to call coconut.call(), coconut.emit(), etc.
```

### coconut.call()

```typescript
await coconut.call<TResponse, TPayload>(
  name: TCommandName,
  payload?: TPayload,
): Promise<TResponse>
```

Calls a Lua command and returns the result.

```js
// With type inference (when commands.d.ts is generated):
const result = await coconut.call("greet", { name: "Ada" })
// result: { greeting: string }

// Without types:
const result = await coconut.call("unknown_command", { data: "test" })
// result: unknown
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

### coconut.emit()

```typescript
await coconut.emit(name: string, payload?: object): Promise<void>
```

Emits an event to the Lua backend.

```js
await coconut.emit("navigate", { view: "settings" })
```

### coconut.on()

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

### coconut.views()

```typescript
await coconut.views(): Promise<string[]>
```

Returns the list of registered view names.

```js
const views = await coconut.views()
// ["home", "settings", "about"]
```

### coconut.ping()

```typescript
await coconut.ping(): Promise<string>
```

Connectivity test. Returns `"pong"`.

```js
const pong = await coconut.ping()
// "pong"
```

### coconut.window

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

### coconut.fs

```typescript
coconut.fs: {
  readText(path: string): Promise<{ ok: boolean; data?: string; error?: string }>
}
```

Filesystem helpers.

```js
const { ok, data, error } = await coconut.fs.readText("/path/to/file.txt")
if (ok) {
  console.log(data)  // File contents
} else {
  console.error(error)
}
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
| `WebViewError` | Webview error | WKWebView/WebView2 failure |
| `ParseError` | Parse error | Invalid JSON in message |

### Error Envelope

Error responses use the `kError` message type:

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

### ObjC Exception Interception

On macOS, native dialog calls (NSOpenPanel, NSSavePanel, NSAlert) are wrapped in `@try/@catch`:

```cpp
// In src/platform/darwin/dialog.mm
Result platformOpenFile(...) {
  Result result{};
  @try {
    // ObjC runtime calls
  } @catch (NSException *e) {
    debug::error(std::format("dialog::openFile ObjC exception: {}", [[e reason] UTF8String]));
  } @catch (...) {
    debug::error("dialog::openFile unknown exception");
  }
  return result;
}
```

### Lua pcall Safety

Example commands use `pcall` for safety around native calls:

```lua
-- In examples/code-editor/commands/editor.lua
local function open_dialog(payload, ctx)
  local ok, result = pcall(coconut.dialog.open, "Open File or Folder", false, true)
  if not ok then
    coconut.error("open_dialog: pcall failed: " .. tostring(result))
    return { cancelled = true, error = tostring(result) }
  end
  if result.confirmed and result.path then
    return { path = result.path, is_dir = result.is_dir }
  end
  return { cancelled = true }
end
```

---

## TypeScript Definitions

### coconut.d.ts

The main type definition file provides ambient typings for `window.coconut`:

```typescript
// scripts/coconut.d.ts
export {};

/// <reference path="./generated/commands.d.ts" />

export type CoconutPayload = Record<string, unknown>;

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
// With generated types:
await coconut.call("greet", { name: "Ada" })  // ✅ Type-checked

// Without generated types:
await coconut.call("typo_command", { ... })    // ❌ Type error
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
