# Coconut Milk v0 API Spec

This document defines the current minimal Lua desktop UI framework API inspired by Tauri/Electron, built on top of a native webview bridge.

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

A view is represented as a table-like descriptor object (optionally with lazy factories) that can also carry controller configuration like `defineProps(...)` and lifecycle callbacks.

In v0/v1, Coconut can attach controller methods (e.g. `on_mount`, `on_unmount`) via a metatable or other Lua mechanisms. The public shape is still an object/table descriptor, not raw window state.

```lua
---@class CoconutViewSpec
---@field kind "url" | "html" | "file"
---@field value string
---@field name? string
---@field meta? table
```

### `coconut.views()`
Returns a table of named views.

### `coconut.events(name, payload, ctx)`
A Love2D-like main dispatcher callback for **frontend → Lua events**.

- called for every frontend-emitted event
- `name` is the frontend event name (event namespace)
- `payload` is the event payload table
- `ctx` is the runtime context

View controller callbacks are attached via methods on the view descriptor, for example:

- `view:defineProps(default_props)` - declares view props defaults
- `view:on_load(fn)` - once when the view is first loaded/created
- `view:on_mount(fn)` - when the view becomes the active/visible view
- `view:on_unmount(fn)` - when switching away from the view
- `view:on_frontend_event(name, fn)` - when a frontend event is emitted while the view is active

In addition, Coconut may also call `coconut.events(name, payload, ctx)` as a global dispatcher for frontend → Lua events.

If navigation switches to the view with props (via `ctx.window:show(name, props)`), the effective props are available inside callbacks as `ctx.props`.

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
- Each value is either:
  - a view descriptor created by `View.url/html/load`, OR
  - a lazy view factory `function() -> view_descriptor` (called when the view is first loaded)

- Names are used for routing and startup selection.
- The runtime switches views by name, not by raw descriptor.
- view props can be passed when switching views (via `ctx.window:show(name, props)`), and callbacks see them as `ctx.props`.

---

## 4. Window and app context

The `ctx` object is the primary runtime API.

### `coconut.config(ctx)`
Receives the runtime context and configures the application.

```lua
function coconut.config(ctx)
  return ctx
    :setWindowSize({ w = 1280, h = 640 })
    :setInitialView("home")
end
```

### Context methods

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

### `coconut.on_resize(ctx, w, h)`
Called when the window is resized.

Example:

```lua
function coconut.on_resize(ctx, w, h)
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
    :setWindowSize({ w = 1280, h = 640 })
    :setInitialView("home")
    :bind("sayHi", function(params, ctx)
      print("Hi " .. (params.name or "user"))
      ctx:emit("greeted", { name = params.name or "user" })
    end)
end

function coconut.on_resize(ctx, w, h)
  -- runtime provides ctx so Lua can emit or dispatch events
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

Coconut uses a **layered bridge**:

1. **RPC envelope** — a canonical message shape that all bridge traffic uses
2. **Transport abstraction** — a pluggable interface for sending/receiving envelopes
3. **Platform transport** — a concrete implementation (WebUI today, webview target)

The Lua API stays stable regardless of which transport is active.

### 11.1 RPC envelope

The bridge uses a single canonical envelope for all JS ↔ C++ communication.

It is defined in `src/rpc_envelope.h` as:

```cpp
namespace coconut::rpc {

enum class Type {
  kCall,    // Request a command call (JS → C++)
  kReturn,  // Successful response to a call (C++ → JS)
  kError,   // Error response to a call (C++ → JS)
  kEvent,   // One-way fire-and-forget (either direction)
  kReady,   // Bridge readiness handshake (JS → C++)
};

struct Message {
  Type        type;     // discriminator
  std::string id;       // empty for fire-and-forget
  std::string name;     // command name, event name, or binding name
  nlohmann::json payload; // params, result, or error details

  nlohmann::json toJson() const;
  static Message fromJson(const nlohmann::json& j);
  static Message fromJson(const std::string& s);
};

} // namespace coconut::rpc
```

#### Envelope JSON shape

```json
{ "type": "call",   "id": "uuid",  "name": "cmd", "payload": {…} }
{ "type": "return", "id": "uuid",                    "payload": <any> }
{ "type": "error",  "id": "uuid",                    "payload": { "code": "…", "message": "…" } }
{ "type": "event",                      "name": "evt", "payload": {…} }
{ "type": "ready" }
```

#### Envelope rules

- `id` is required for `call`, `return`, and `error`
- `name` is required for `call` and `event`
- `payload` is a JSON value (object, array, or primitive)
- `error` is only used on failure envelopes
- the `type` field is the primary discriminator
- `ready` has no `id` and carries no payload
- `event` is used in both directions; its namespace is separate from commands

### 11.2 Transport abstraction

The transport interface is defined in `src/transport.h`:

```cpp
namespace coconut::transport {

using MessageCallback = std::function<void(const rpc::Message&)>;

class Transport {
public:
  virtual ~Transport() = default;
  virtual void send(const rpc::Message& msg) = 0;
  virtual void setMessageCallback(MessageCallback cb) = 0;
};

} // namespace coconut::transport
```

Concrete implementations:

| Transport | Mechanism | Direction |
|---|---|---|
| `WebuiTransport` (current) | `webui_run()` → JS eval, `webui_bind()` ← WebSocket callback | Outgoing via eval, incoming via bound function |
| `WebviewTransport` (target) | `webview_eval()` → JS, `webview_bind()` ← native WKScriptMessageHandler | Outgoing via eval, incoming via native handler |

### 11.3 Transport ownership

The transport is owned by `bridge::State` and created via `bridge::createTransport(app)`:

```cpp
// bridge::State now carries a Transport pointer
struct State {
  Config*               configs    = nullptr;
  transport::Transport* transport  = nullptr;  // owned
};

// Factory
void createTransport(App* app);
```

`createTransport()`:
1. Creates a platform-specific transport (currently `WebuiTransport`)
2. Stores it on `app->bridge_state->transport`
3. Sets up the frontend binding via `setupEmitBinding()`
4. In `lua_runtime.cpp`, the call changes from `bridge::setupEmitBinding()` → `bridge::createTransport()`

The transport is destroyed in `bridge::destroy()`, which deletes the concrete transport before freeing the state.

#### Outgoing RPC helper

```cpp
void rpcSend(App* app, const rpc::Message& msg);
```

This is the preferred way to send any RPC message from C++ to JS. It looks up the transport from `app->bridge_state` and calls its `send()` method.

### 11.4 Legacy path (pre-refactor)

Existing bridge functions (`emitToJS`, `callJS`, `emitToLua`, `callLua`) continue to work as direct wrappers around WebUI calls. They do not use the transport layer.

Migration plan:

| Phase | Change |
|---|---|
| Current | Both legacy and RPC paths coexist. `createTransport()` sets up the transport + binding. Legacy functions (`emitToJS`, `callJS`) bypass the transport. |
| webview migration | `emitToJS` / `callJS` rewritten to use `transport->send(RpcMessage)`. A `WebviewTransport` replaces `WebuiTransport`. |
| Complete | Legacy path removed. All C++ ↔ JS traffic goes through the transport interface. |

### 11.5 Message types

#### `call`
Frontend → Lua command invocation. Used when JavaScript calls `coconut.call(name, payload)`.

- `id` is required for promise tracking
- `name` is the registered command name
- `payload` is the params table

#### `return`
Successful response to a `call`. C++ → JS.

- `id` must match the call's id
- `payload` is the return value from the Lua command

#### `error`
Error response to a `call`. C++ → JS.

- `id` must match the call's id
- `payload` is an object with `code`, `message`, and optional `details`

#### `event`
A one-way fire-and-forget message in either direction.

Used by:
- `ctx:emit(name, payload)` — Lua → frontend
- `coconut.emit(name, payload)` — frontend → Lua
- `__coconut_dispatch_event(name, payloadJson)` — injected JS runtime

#### `ready`
Sent by the frontend when the bridge initializes. See frontend readiness below.

### 11.6 Lua → frontend flow

`ctx:emit(name, payload)`

- async by default
- serializes a `kEvent` RPC message
- queues it for delivery
- returns immediately

`ctx:emit_sync(name, payload)`

- serializes the same `kEvent` envelope
- sends it immediately
- blocks until the dispatch step completes
- may return an error if the frontend is not ready

### 11.7 Frontend → Lua flow

Bound commands are invoked from the frontend using the command name registered by `ctx:bind(name, fn)`.

Rules:

- one command name maps to one handler
- payloads are Lua tables on the Lua side
- the frontend should send object-like payloads
- the runtime dispatches the message to the registered Lua function
- the frontend call returns a `Promise`
- the promise resolves with the Lua return value or rejects with an error

### 11.7.1 Public frontend API

Coconut exposes a small base frontend API:

```ts
await coconut.ready()
await coconut.call("sayHi", { name: "Ada" })
const unsub = coconut.on("toast", (payload) => {
  console.log(payload.message)
})
await coconut.emit("frontend_ready", { at: Date.now() })
```

Generated command helpers wrap this base API:

```ts
import { sayHi } from "./commands/hello.g"

await sayHi({ name: "Ada" })
```

Generated type shape:

```ts
export type CoconutCommandName = "sayHi" | "hello" | "note"

interface CoconutFrontendAPI<TCommandName extends string = CoconutCommandName> {
  call(name: TCommandName, payload: object): Promise<unknown>
}
```

### Bridge payload encoding (v1)

Across the bound functions, Coconut standardizes payload transport:

- every `payload` is serialized as a **JSON string** in JavaScript
- C++ parses the JSON string into Lua tables
- Lua → C++ payloads are serialized back into JSON strings for JavaScript

### `coconut.call(...)` response envelope

`coconut.call(name, payload)` relies on a C++ implementation of `__coconut_call(name, payloadJson)`.

The C++ side returns a single JSON string envelope:

- success:
  - `{ "ok": true, "data": <lua/js value> }`
- failure:
  - `{ "ok": false, "error": { "code": string, "message": string, "details"?: unknown } }`

JavaScript `coconut.call(...)` resolves with `data` on success and rejects with the decoded error on failure.

#### `coconut.ready()`
Returns a `Promise<void>` that resolves when the bridge handshake has completed.

#### `coconut.on(name, fn)`
Registers a frontend listener for a named Lua-emitted event.

Runtime glue: Coconut's injected JS calls `__coconut_dispatch_event(name, payloadJson)`.

- `name` is the event name (event namespace)
- `payloadJson` is the JSON-string serialized payload
- Coconut parses `payloadJson` and invokes registered `on(...)` callbacks

#### `coconut.emit(name, payload)`

Sends a frontend-originated event into the bridge.

- async
- returns a `Promise<void>`
- success means the event was accepted/forwarded by the runtime
- no response payload is returned
- the promise rejects if the frontend cannot notify the Lua side

Payload encoding rule (v1):
- `payload` is a Lua/JS object in app code
- across the bridge boundary it is serialized as a **JSON string**
- C++ parses it back into a Lua table

Ack semantics (v1):
- `coconut.emit(...)` relies on `__coconut_emit(name, payloadJson)`
- `__coconut_emit` may return an empty/undefined value for success
- if it returns a JSON envelope string, the envelope must match the `coconut.call(...)` failure form (`{ ok:false, error: ... }`) and the JS `emit` Promise rejects accordingly

#### Namespace separation

Coconut keeps command names and event names in separate namespaces.

- command names are used by `coconut.call(...)` and `ctx:bind(...)`
- event names are used by `coconut.on(...)`, `coconut.emit(...)`, and `ctx:emit(...)`
- the same string may exist in both namespaces without conflict, although that should generally be avoided for clarity

#### `coconut.on(name, fn)` return value
Returns an unsubscribe function.

```ts
type CoconutUnsubscribe = () => void
```

### 11.8 Frontend readiness

Coconut uses a **two-way handshake** to determine when the bridge is active.

#### Handshake summary

1. C++ creates the bridge and loads the frontend.
2. The frontend bridge script initializes.
3. The frontend sends `ready` (a `kReady` RPC message).
4. The runtime acknowledges that the bridge is active.

#### Startup behavior before `ready`

- async Lua → frontend events are queued
- `coconut.emit(...)` queues until the bridge is ready
- `coconut.call(...)` waits until the bridge is ready, then proceeds
- sync operations may fail if the bridge is not available
- generated helpers should wait for readiness rather than fail immediately when possible

#### `ready` message

The frontend-side Promise returned by `coconut.ready()` resolves when the runtime calls `__coconut_bridge_ready()`.

Implementation note:
- `__coconut_bridge_ready()` is a global function that Coconut's injected JS exposes
- C++ calls it after the bridge dispatcher is installed and the bound functions are usable

### 11.8.1 Pre-ready buffering rules

- event queues are allowed before readiness
- command calls wait for readiness
- queued events must preserve order per bridge context
- if the queue overflows or the bridge never becomes ready, the runtime should reject with a bridge error instead of silently dropping messages

### 11.9 Error handling

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

```lua
---@class CoconutBridgeError
---@field code string
---@field message string
---@field details? table
```

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

### 11.10 Implementation status

#### Current (WebUI)

- RPC envelope types defined in `rpc_envelope.h` ✓
- Transport interface defined in `transport.h` ✓
- `WebuiTransport` implemented in `bridge.cpp` ✓
- `bridge::createTransport(app)` creates transport + binding ✓
- Legacy `emitToJS` / `callJS` still bypass transport (next phase)
- Transport cleanup in `bridge::destroy()` ✓

#### Next (post-migration to webview)

- `WebviewTransport` for native WKWebView
- All outbound messages use `transport->send(RpcMessage)`
- Legacy path removed
- JS protocol unified under `__coconut_rpc_receive` and `__coconut_rpc`

---

## 12. v0 summary

### Fixed in v0

- single-window first
- named views
- `views/` default root
- `commands/` folder
- `View.url`, `View.html`, `View.load`
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

---

## 13. Custom URL scheme (`coconut://`)

Coconut registers a custom `coconut://` URL scheme so views can reference
assets with portable paths that resolve from the app root directory,
regardless of where the HTML file lives on disk.

### Purpose

Without a custom scheme, file-based views (`View.load()`) served via
`webview_navigate("file://...")` resolve relative paths from the HTML
file's directory. This forces HTML authors to use fragile relative paths
like `../assets/style.css` that break if views are reorganised.

The `coconut://` scheme provides a clean, filesystem-independent path:

```html
<link rel="stylesheet" href="coconut://assets/style.css">
<script src="coconut://assets/script.js"></script>
<img src="coconut://assets/icon.png">
```

All paths are resolved relative to the **application root directory**
(the working directory at startup).

### Platform backends

| Platform | Backend | Mechanism |
|----------|---------|----------|
| macOS | WKWebView | `WKURLSchemeHandler` (registered on `WKWebViewConfiguration` before webview creation) |
| Windows | WebView2 | `CoreWebView2.AddWebResourceRequestedFilter()` + `WebResourceRequested` event (stub in v0.1) |
| Linux | WebKitGTK | `webkit_web_context_register_uri_scheme()` (stub in v0.1) |

### macOS implementation

On macOS (WKWebView), the scheme handler **must** be registered before the
WKWebView is created, because `WKURLSchemeHandler` is set on the
`WKWebViewConfiguration` which is read-only after webview construction.

Coconut adds a minimal hook to the webview library's `cocoa_webkit.hh`:

```cpp
// cocoa_wkwebview_engine.hh (webview third-party library)
inline static std::function<void(id)> on_configure_config;
```

This is called inside `window_settings()` right before the WKWebView is
created, giving Coconut a chance to register its scheme handler:

```mm
[config setURLSchemeHandler:handler forURLScheme:@"coconut"];
```

The handler implementation lives in
`src/platform/darwin/scheme_handler.mm`:

1. A lightweight ObjC class (`CoconutSchemeHandler`) is created at
   runtime using `objc_allocateClassPair` / `class_addMethod` (no ARC
   conflicts with the webview library's manual retain/release).
2. Two protocol methods are implemented:
   - `webView:startURLSchemeTask:` — resolves the URL path against the
     app root, reads the file via `coconut::fs::readBytes()`, determines
     the MIME type from the file extension, and responds with the data.
   - `webView:stopURLSchemeTask:` — no-op (cancellation not needed).
3. The handler is registered on the `WKWebViewConfiguration` via
   `setURLSchemeHandler:forURLScheme:`.

### Startup flow

1. `main.cpp` calls `platform::installSchemeHandlerHook(root_dir)`
   **before** `app::create()` / `webview_create()`.
2. On macOS, this sets `cocoa_wkwebview_engine::on_configure_config`.
3. When `webview_create()` constructs the engine, `window_settings()`
   calls the hook, which calls `registerHandler(config)`.
4. `registerHandler()` creates the `CoconutSchemeHandler` ObjC class
   (lazily, once) and registers it on the WKWebViewConfiguration.
5. When a view requests `coconut://assets/style.css`, WKWebView calls
   `startURLSchemeTask`, which reads the file and returns it.

### Path resolution

`coconut://assets/style.css` → strip scheme → `assets/style.css` →
resolve against app root → `/abs/path/to/app/assets/style.css`

Uses `coconut::fs::resolve()` internally.

### File types and MIME

The handler maps common extensions to MIME types (css, js, html, png,
jpg, gif, svg, ico, woff, woff2, ttf). Unknown extensions default to
`application/octet-stream`.

### Future

- **Windows**: Implement via `CoreWebView2.WebResourceRequested` event.
- **Linux**: Implement via `webkit_web_context_register_uri_scheme()`.
- **Memory caching**: Optional in-memory cache for frequently-requested
  assets (CSS, JS bundles).
- **Virtual filesystem**: Support for embedded assets (e.g. from a
  compiled-in ZIP or a resource section).
