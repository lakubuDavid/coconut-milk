# Coconut Milk v0 API Spec

This document defines the current minimal Lua desktop UI framework API inspired by Tauri/Electron, built on top of WebUI.

The goal of v0 is to keep the surface area small, predictable, and easy to generate tooling from later.

---

## 1. Design goals

- Single-window first.
- Named views only.
- Minimal runtime API.
- Explicit command binding.
- Build-time generation for editor support and binding glue.
- Lua tables only for payloads.
- Views and commands should be easy to scan from a project layout.

---

## 2. Current project shape

### Application entry file

The app exposes a `coconut` global module contract.

```lua
function coconut.views()
end

function coconut.config(ctx)
end
```

### Default directories

- `views/` for HTML/view assets
- `commands/` for command modules
- `assets/` for static framework assets
- generated files can live under a build output directory, for example `build/generated/`

These defaults may be configurable later. For now they are part of the v0 convention.

---

## 3. View system

### Module

```lua
local View = require "coconut-milk.view"
```

> Note: the module name in the sample is currently misspelled as `coconut-milk.viwe`. The intended public API should be `coconut-milk.view`.

### View factories

The `View` module is a pure descriptor factory. It does not create live window state.

#### `View.url(url)`
Creates a view descriptor for a remote URL.

#### `View.html(html)`
Creates a view descriptor for inline HTML content.

#### `View.load(path)`
Creates a view descriptor for a local file.

#### Path resolution
`View.load(path)` resolves relative paths from the framework asset root.

Default resolution in v0:

- `assets/note.html` for `View.load("note.html")`

A future version may allow changing the asset root, but the default is fixed in v0.

### View descriptor shape

A view is represented as a plain Lua table.

```lua
---@class CoconutViewSpec
---@field kind "url" | "html" | "file"
---@field value string
---@field name? string
---@field meta? table
```

### `coconut.views()`
Returns a table of named views.

```lua
function coconut.views()
  return {
    home = View.url("https://example.com"),
    hello = View.html("<html><body>Hello</body></html>"),
    note = View.load("note.html"),
  }
end
```

#### Rules

- Each key is a view name.
- Names are used for routing and startup selection.
- The runtime switches views by name, not by raw descriptor.

---

## 4. Window and app context

The `ctx` object is the primary runtime API.

### `coconut.config(ctx)`
Receives the runtime context and configures the application.

```lua
function coconut.config(ctx)
  return ctx
    :setBrowser("auto")
    :setWindowSize({ w = 1280, h = 640 })
    :setInitialView("home")
end
```

### Context methods

#### `ctx:setBrowser(mode)`
Sets the browser backend.

Example values:

- `"auto"`
- backend-specific names such as `"webkit"`, `"webview2"`, etc.

#### `ctx:setWindowSize(size)`
Sets the initial window size.

```lua
ctx:setWindowSize({ w = 1280, h = 640 })
```

Expected shape:

```lua
---@class CoconutWindowSize
---@field w integer
---@field h integer
```

#### `ctx:setInitialView(name)`
Sets the initial active view by name.

#### `ctx:show(name)`
Switches the active view by name.

#### `ctx:reload()`
Reloads the current active view.

#### `ctx:close()`
Requests application or window shutdown.

#### Chainability
The following methods are chainable in v0:

- `setBrowser`
- `setWindowSize`
- `setInitialView`

The following methods are action methods and may return a status value later:

- `bind`
- `emit`
- `emit_sync`
- `show`
- `reload`
- `close`

---

## 5. Event model

### `ctx:emit(name, payload)`
Sends an event to the frontend asynchronously.

- async by default
- queue-based delivery
- preserves order per context

### `ctx:emit_sync(name, payload)`
Sends an event immediately.

- blocking semantics from the caller perspective
- should not return until the dispatch step is complete
- used for cases where immediate delivery is required

### Payload rule
In v0, payloads are Lua tables only.

- use `{}` for empty payloads
- do not pass primitive values as payloads in v0

Example:

```lua
ctx:emit("toast", { message = "saved" })
ctx:emit_sync("ping", { at = os.time() })
```

---

## 6. Command bindings

### Core rule
One command name maps to one handler in v0.

Duplicate binds for the same name should be treated as an error.

### `ctx:bind(name, fn)`
Registers a single command handler.

```lua
ctx:bind("sayHi", function(params, ctx)
  print("Hi " .. (params.name or "user"))
end)
```

### Handler signature

```lua
---@alias CoconutCommandFn fun(params: table, ctx: CoconutContext)
```

#### Parameters
- `params` is always a table
- `ctx` is the runtime context

### Command naming
The public command name is the string passed to `ctx:bind`.

Example:

```lua
ctx:bind("hello", hello)
```

### Error handling
- duplicate command registration should fail fast
- missing command names should be rejected
- invalid handler values should be rejected

---

## 7. Command files and build output

The framework supports a `commands/` folder with build-time generation.

### Source convention
The author-written command module contains the command implementation and annotations.

For v1, the source module is the **pure implementation source**. It should not perform the runtime binding itself.

A single source file may export **multiple commands**.

The build pipeline uses the **LuaLS annotation syntax** as the canonical marker syntax, following the conventions described by LuaLS annotations.

Reference: https://luals.github.io/wiki/annotations/#understanding-this-page

Coconut adds the custom `@command` tag on top of the LuaLS annotation set.

#### Marker placement rules

- `---@command` must be placed immediately above the function it annotates
- the annotated function should be a plain Lua function definition
- a single source file may contain multiple `---@command` blocks
- the export table should expose the same command function names used by the annotations

#### Preprocessor scan rule

Coconut's preprocessor will implement a very basic parser.

It scans from the line containing `---@command NAME` through the annotated function signature, stopping at the closing `)` of the parameter list.

In practical terms, it only needs enough structure to capture patterns like:

```lua
---@command hello
---@param params { name?: string }
local function hello(arg0, arg1)
```

Multiline function signatures are allowed as long as the closing `)` can be found.
Both `function` and `local function` are valid targets.

The function signature is the only fallback source of truth for public argument names.

If `---@param` is missing or incomplete, the generator should infer the callable argument names from the function signature and fill missing types with `any`.

### Type inference rules

- `---@param` defines the public parameter type when present
- if `---@param` is missing, infer parameter names from the function signature and use `any`
- if a parameter exists in the signature but not in annotations, generate it as `any`
- if annotations name parameters that do not exist in the function signature, ignore those extra annotation names
- `---@return` defines the public return type when present
- if `---@return` is missing or unknown, use `any`
- `any` is the safe fallback for incomplete or ambiguous type information
- the function signature is the fallback source of truth for public argument names

The parser does **not** need to read the function body in v1.
It only needs to associate the annotation with the function signature and extract the exported command name.

Example source file:

```lua
---@command hello
---@param params { name?: string }
---@return string
local function hello(params, ctx)
  local name = (params and params.name) or "user"
  local message = "Hi " .. name

  if ctx and ctx.emit then
    ctx:emit("greeted", { name = name })
  end

  return message
end

---@command goodbye
---@param params { name?: string }
---@return string
local function goodbye(params, ctx)
  local name = (params and params.name) or "user"
  return "Bye " .. name
end

return {
  hello = hello,
  goodbye = goodbye,
}
```

### Build artifacts
From `commands/hello.lua`, the build may generate:

- `commands/hello.d.ts`
- `commands/hello.g.lua`
- `commands/hello.g.ts` or `commands/hello.g.js` for frontend helpers

### Generated file roles

#### `.d.ts`
Provides editor/type support and typed frontend usage.

#### `.g.lua`
Generated binding glue that performs the actual runtime registration.

It should export a single registration function that binds all discovered command(s) to `ctx`.

Example generated shape:

```lua
-- Generated by Coconut Milk build pipeline.
-- Do not edit by hand.

local impl = require("commands.example")

---@type fun(ctx: CoconutContext)
local function register(ctx)
  ctx:bind("hello", impl.hello)
  ctx:bind("goodbye", impl.goodbye)
  return ctx
end

return register
```

The runtime may load this file instead of the author-written source file.

#### `.g.ts` / `.g.js`
Generated frontend helper module for calling the command from JavaScript or TypeScript.

These helpers should be thin wrappers around `coconut.call(...)`.

### Generated helper module shape

For v1, each generated helper module should export a **single callable helper** per command.

Example generated shape:

```ts
export type SayHiParams = { name?: string }
export type SayHiResult = string

export declare function sayHi(payload: SayHiParams): Promise<SayHiResult>
export default sayHi
```

Rules:

- one command file may generate one or more callable helpers
- helper names should match the bound command name when possible
- helper functions are thin wrappers around `coconut.call(...)`
- the base `coconut.call(...)` API remains the generic fallback for dynamic command names
- generated helper modules may also export command-name unions for autocomplete and narrowing
- generated frontend helpers and generated Lua glue should be derived from the same annotations
- source command modules are implementation-only; generated glue performs binding
- LuaLS annotations are the canonical markers for command detection and type extraction
- Coconut adds a custom `@command` tag to the LuaLS annotation set

### Annotation role
Annotations are metadata for the build pipeline.

They may later evolve into a more Rust/C#-style attribute system, but in v0 they are optional and build-oriented.

---

## 8. Lifecycle hooks

### `coconut.on_resize(w, h)`
Called when the window is resized.

Example:

```lua
function coconut.on_resize(w, h)
  ctx:emit("on_resize", { w = w, h = h })
end
```

### Hook contract
Lifecycle hooks are app-level functions on the `coconut` module.

Possible hooks in later versions may include:

- `coconut.on_ready(ctx)`
- `coconut.on_close(ctx)`
- `coconut.on_focus()`
- `coconut.on_blur()`

These are not part of the strict v0 runtime contract yet.

---

## 9. Example v0 app

```lua
local View = require "coconut-milk.view"

function coconut.views()
  return {
    home = View.url("https://example.com"),
    hello = View.html("<html><body>Hello</body></html>"),
    note = View.load("note.html"),
  }
end

function coconut.config(ctx)
  return ctx
    :setBrowser("auto")
    :setWindowSize({ w = 1280, h = 640 })
    :setInitialView("home")
    :bind("sayHi", function(params, ctx)
      print("Hi " .. (params.name or "user"))
      ctx:emit("greeted", { name = params.name or "user" })
    end)
end

function coconut.on_resize(w, h)
  -- runtime may provide the current ctx or a dedicated event bridge later
end
```

---

## 10. Things intentionally left open

The following parts are not specified yet and will be discussed next:

- the bridge between WebUI and Lua
- serialization format details
- transport directionality
- whether frontend calls are pull, push, or request/response
- scheduling model for queued events
- thread ownership and re-entrancy
- how `emit_sync` is implemented internally
- how command annotations are parsed in the build pipeline

---

## 11. Bridge design v0

Coconut uses a **hybrid bridge**:

- WebUI provides the underlying transport and callback primitives.
- Coconut defines a small normalized message contract on top of it.
- The Lua API stays stable even if the WebUI backend changes later.

### 11.1 Transport layer

The transport layer is based on WebUI's native capabilities:

- native -> frontend uses WebUI script execution
- frontend -> native uses WebUI callback/bind mechanisms

Coconut should not expose WebUI transport details directly to application code.

### 11.2 Public bridge contract

The bridge uses a JSON envelope for messages.

#### Common envelope fields

```lua
---@class CoconutBridgeMessage
---@field type string
---@field id? string|integer
---@field name? string
---@field payload? table
---@field error? table
```

### 11.3 Message types

#### `ready`
Sent by the frontend when the bridge is initialized and ready to receive messages.

#### `event`
A one-way message from Lua to frontend.

Used by `ctx:emit(name, payload)`.

#### `event_sync`
A blocking Lua to frontend message.

Used by `ctx:emit_sync(name, payload)`.

#### `call`
A frontend to Lua command invocation.

Used when JavaScript requests a bound Lua command.

The frontend receives a `Promise` for the result.

#### `return`
A response message for request/response flows.

Used when the frontend or Lua side expects a value back.

Promise resolution on the frontend is completed with this message.

#### `error`
An error response for failed dispatches or malformed payloads.

### 11.4 Lua -> frontend flow

`ctx:emit(name, payload)`

- async by default
- serializes a message envelope
- queues it for delivery
- returns immediately

`ctx:emit_sync(name, payload)`

- serializes the same envelope
- sends it immediately
- blocks until the dispatch step completes
- may return an error if the frontend is not ready

### 11.5 Frontend -> Lua flow

Bound commands are invoked from the frontend using the command name registered by `ctx:bind(name, fn)`.

Rules:

- one command name maps to one handler
- payloads are Lua tables on the Lua side
- the frontend should send object-like payloads
- the runtime dispatches the message to the registered Lua function
- the frontend call returns a `Promise`
- the promise resolves with the Lua return value or rejects with an error

### 11.5.1 Public frontend API

Coconut should expose a small base frontend API, for example:

```ts
await coconut.ready()
await coconut.call("sayHi", { name: "Ada" })
const unsub = coconut.on("toast", (payload) => {
  console.log(payload.message)
})
await coconut.emit("frontend_ready", { at: Date.now() })
```

Generated command helpers should wrap this base API rather than replace it.

Generated helper modules should also export a union of known command names for autocomplete and type narrowing.

Example future usage:

```ts
import { sayHi } from "./commands/hello.g"

await sayHi({ name: "Ada" })
```

This keeps `coconut.call(...)` available for generic or dynamic calls while still giving typed helpers for known commands.

Suggested generated type shape:

```ts
export type CoconutCommandName = "sayHi" | "hello" | "note"

interface CoconutFrontendAPI<TCommandName extends string = CoconutCommandName> {
  call(name: TCommandName, payload: object): Promise<unknown>
}
```

#### `coconut.ready()`
Returns a `Promise<void>` that resolves when the bridge handshake has completed and both sides are ready.

Generated helpers and manual calls may await this before issuing bridge traffic.

#### `coconut.on(name, fn)`
Registers a frontend listener for a named Lua-emitted event.

#### `coconut.emit(name, payload)`
Sends a frontend-originated event into the bridge.

- async
- returns a `Promise<void>`
- success means the event was accepted/forwarded by the runtime
- no response payload is returned
- the promise rejects if the frontend cannot notify the Lua side

### 11.5.2 Namespace separation

Coconut keeps command names and event names in separate namespaces.

- command names are used by `coconut.call(...)` and `ctx:bind(...)`
- event names are used by `coconut.on(...)`, `coconut.emit(...)`, and `ctx:emit(...)`
- the same string may exist in both namespaces without conflict, although that should generally be avoided for clarity

#### `coconut.on(name, fn)` return value
Returns an unsubscribe function.

Suggested TypeScript type:

```ts
type CoconutUnsubscribe = () => void
```

Example:

```ts
const unsub = coconut.on("toast", (payload) => {
  console.log(payload.message)
})

unsub()
```

### 11.6 Frontend readiness

Coconut uses a **two-way handshake** to determine when the bridge is active.

#### Handshake summary

1. C++ creates the bridge and loads the frontend.
2. The frontend bridge script initializes.
3. The frontend sends `ready`.
4. The runtime acknowledges that the bridge is active.

#### Startup behavior before `ready`

- async Lua -> frontend events are queued
- `coconut.emit(...)` queues until the bridge is ready
- `coconut.call(...)` waits until the bridge is ready, then proceeds
- sync operations may fail if the bridge is not available
- generated helpers should wait for readiness rather than fail immediately when possible

#### `ready` message

`ready` is the frontend's signal that its bridge layer is initialized and able to participate in message exchange.

The runtime may use the handshake to establish that both sides are prepared before allowing normal traffic.

### 11.6.1 Pre-ready buffering rules

- event queues are allowed before readiness
- command calls wait for readiness
- queued events must preserve order per bridge context
- if the queue overflows or the bridge never becomes ready, the runtime should reject with a bridge error instead of silently dropping messages

### 11.7 Error handling

Coconut uses **error values** instead of exceptions for normal recoverable failures.

C++ code should prefer `std::expected<T, Error>` (or an equivalent value-returning error path) for operations that can fail and need to report why.

The shared error vocabulary is a small value type:

```cpp
enum class ErrorCode {
  Ok,
  Unknown,
  InvalidConfig,
  InvalidView,
  MissingFile,
  DuplicateCommand,
  CommandNotFound,
  InvalidPayload,
  NotReady,
  QueueOverflow,
  LuaError,
  BridgeError,
  WebUiError,
  ParseError,
};

struct Error {
  ErrorCode code;
  std::string message;
  std::string details;
};
```

Bridge errors should be normalized into a structured error table.

Suggested shape:

```lua
---@class CoconutBridgeError
---@field code string
---@field message string
---@field details? table
```

Suggested TypeScript shape:

```ts
export interface CoconutError {
  code: string
  message: string
  details?: unknown
}
```

If a frontend command promise is rejected, the rejection reason should be derived from this structure.

### C++ error handling guidance

- use `std::expected` for recoverable failures when possible
- reserve exceptions for truly exceptional/unrecoverable situations
- use `ErrorCode` for machine-readable branching
- use `message` for short human-readable summaries
- use `details` for optional context, logs, or serialization hints

### 11.8 Wire envelope by use case

The wire envelope is the internal bridge contract used by Coconut's runtime and generated helpers.

It is **conceptual and object-shaped**. The runtime may represent it natively in C++/Lua/JS and does not need to stringify it as JSON unless the transport requires that.

Suggested TypeScript shapes:

```ts
export type CoconutWireCall = {
  type: "call"
  id: string
  name: string
  payload: Record<string, unknown>
}

export type CoconutWireReturn<T = unknown> = {
  type: "return"
  id: string
  payload: T
}

export type CoconutWireError = {
  type: "error"
  id: string
  error: CoconutError
}

export type CoconutWireEvent = {
  type: "event"
  name: string
  payload: Record<string, unknown>
}

export type CoconutWireReady = {
  type: "ready"
}
```

#### `call`

```lua
---@class CoconutWireCall
---@field type "call"
---@field id string
---@field name string
---@field payload table
```

Used for frontend -> Lua command requests.

#### `return`

```lua
---@class CoconutWireReturn
---@field type "return"
---@field id string
---@field payload any
```

Used for successful responses to `call`.

#### `error`

```lua
---@class CoconutWireError
---@field type "error"
---@field id string
---@field error CoconutBridgeError
```

Used when a `call` or a bridge operation fails.

#### `event`

```lua
---@class CoconutWireEvent
---@field type "event"
---@field name string
---@field payload table
```

Used for Lua -> frontend events and frontend -> Lua signaling.

#### `ready`

```lua
---@class CoconutWireReady
---@field type "ready"
```

Used by the frontend to announce the bridge is ready.

### 11.9 Envelope rules

- `id` is required for `call`, `return`, and `error`
- `name` is required for `call` and `event`
- `payload` is a table for all bridge payload-bearing messages in v0
- `error` is only used on failure envelopes
- the `type` field is the primary discriminator
- the same message type should not reuse an `id`
- `ready` has no `id` and carries no payload
- `event` is used in both directions for signaling, but its namespace is separate from commands

### 11.10 v0 implementation intent

In v0, the bridge should remain simple:

- avoid exposing WebUI-specific concepts in the public Lua API
- keep the protocol stable
- keep payloads as tables only
- keep the frontend protocol small enough to version later
- treat JSON as a conceptual schema, not a required internal representation

---

## 12. v0 summary

### Fixed in v0

- single-window first
- named views
- `views/` default root
- `commands/` folder
- `View.url`, `View.html`, `View.load`
- `ctx:setBrowser`
- `ctx:setWindowSize`
- `ctx:setInitialView`
- `ctx:show`
- `ctx:bind`
- `ctx:emit`
- `ctx:emit_sync`
- table-only payloads
- one handler per command name
- generated `.d.ts`, `.g.lua`, and frontend helpers
- base `coconut.ready()` API available
- base `coconut.call(...)` API remains available and can be specialized with a command-name union
- generated helper modules export a single callable helper per command
- frontend `coconut.on(...)` listener API
- frontend `coconut.emit(...)` signaling API
- hybrid WebUI bridge

### To be designed next

- Lua callback dispatch details
- frontend readiness and handshake details
- scheduling model for queued events
- thread ownership and re-entrancy
- how `emit_sync` waits internally
- how command annotations are parsed in the build pipeline
- exact promise payload/return serialization rules
- generated frontend helper module shape
- generated command-name union export
- per-command generated param/return typing
- transport-level ack serialization for `emit`
