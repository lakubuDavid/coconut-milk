# bridge-protocol

## What it is
The canonical RPC envelope for all JS ↔ C++ message passing. Every bridge
message — whether it's a command call, event, error, or readiness signal — uses
the same JSON shape defined in `src/rpc_envelope.h`.

## Why we use it
- Single envelope shape simplifies serialization, logging, and debugging
- Both legacy (script injection) and native (webview_bind) transports use the
  same envelope — only the send/recv mechanism differs
- Type discriminator (`call` / `return` / `error` / `event` / `ready`) routes
  messages without inspecting payload

## Envelope shape

```jsonc
// call:     JS → C++ (request a command)
{ "type": "call",   "id": "uuid", "name": "greet", "payload": { "name": "Ada" } }

// return:   C++ → JS (successful response)
{ "type": "return", "id": "uuid", "payload": "Hello, Ada!" }

// error:    C++ → JS (failed call)
{ "type": "error",  "id": "uuid", "payload": { "code": "LuaError", "message": "..." } }

// event:    either direction (fire-and-forget)
{ "type": "event", "name": "toast", "payload": { "message": "saved" } }

// ready:    JS → C++ (bridge readiness handshake)
{ "type": "ready" }
```

## Key concepts
- **`id`** is empty for fire-and-forget messages (`event`, `ready`)
- **`name`** is the command name, event name, or binding name
- **`payload`** is any JSON value (object, array, string, number, null)

## How we use it here
- `WebviewTransport::static_on_rpc` parses inbound `__coconut_rpc` messages
  into `rpc::Message` and dispatches to `handleCall` / `handleEvent`
- `bridge::rpcSend` sends any `rpc::Message` through the transport's `send()`
- `coconut.js` on the JS side builds the same envelope shape for
  `__coconut_call` and `__coconut_emit`
- Background command results (`CommandResult`) are re-wrapped into `kReturn` /
  `kError` messages before being sent to JS

## Gotchas
- **`webview_bind` wraps args in a JSON array**: the inbound callback receives
  `req = JSON.stringify([msgJson])`, so we parse `args[0]` to get the actual
  envelope
- **Escape sequences**: `escapeJsSingleQuotedString` handles `\`, `'`, `\n`,
  `\r`, `\t` before embedding JSON in JS `webview_eval` strings
- **Error swallowing**: Lua-bound functions that throw must be caught and
  wrapped in a `kError` envelope — never let exceptions propagate to the
  webview callback (see [ADR scope](../decisions/))
