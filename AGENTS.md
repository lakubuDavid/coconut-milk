# Coconut Milk — Agent Guide

Rules this agent must follow when writing code or making decisions.

## Core design decisions

- Single-window first.
- Lua is the application authoring language.
- WebUI is the native webview bridge.
- sol2 is the C++ ↔ Lua binding layer.
- Coconut owns the higher-level runtime protocol.
- Command bindings are explicit and one name maps to one handler.
- Views are named and routed by name.
- Payloads are Lua tables only for the v1 bridge.
- Generated files are part of the build pipeline:
  - `.g.lua` for runtime glue
  - `.d.ts` for typing
  - `.g.ts` / `.g.js` for frontend helpers

## C++ style

- Prefer a C-like, Google based style with `struct`s and namespaces.
- Avoid heavy class hierarchies unless there is a strong reason.
- Use `create(...)` / `destroy(...)` pairs for modules when ownership needs to be explicit.
- Keep module state in small structs.
- Prefer free functions inside namespaces for behavior.
- Keep config as a shared startup object.
- Pass config by pointer/reference, not by value, when modules share the same runtime config.
- Prefer `std::expected<T, Error>` for recoverable failures.
- Be defensive, use error as values where possible, try catch where somehing may fail, bubble up the errors
- Use `ErrorCode` + `Error` as the shared error vocabulary.
- Avoid exceptions for normal control flow.

## Module layout

Each module should follow the same basic shape where applicable:

- `create(Config *config)`
- `destroy(T *state)`

`Config` is created once and shared across modules.

Current modules:
`coconut::app`, `coconut::bridge`, `coconut::lua`, `coconut::webui`,
`coconut::commands`, `coconut::fs`, `coconut::error`

## Lua and bridge style

- Keep the Lua surface minimal and explicit.
- `coconut.config(ctx)` is the startup config hook.
- `coconut.views()` returns named view descriptors.
- `ctx` is the runtime context object passed into Lua.
- Bridge messages are conceptual object-shaped envelopes.
- `emit` is async and queue-based.
- `call` is Promise-based and waits for readiness.

## When in doubt

- Prefer the current spec over inventing new abstractions.
- Prefer minimal state and explicit ownership.
- Ask before making irreversible or ambiguous design changes.
