# Coconut Milk Agent Guide

This file documents the coding style, design decisions, and reference docs for the Coconut Milk project.

## Reference docs

Read these before making architectural or API changes:

- `docs/0-specs.md` — current API and bridge spec
- `docs/1-roadmap.md` — implementation roadmap
- `docs/2-test-suite.md` — test plan and test scope

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

The current module style is:

- `coconut::app`
- `coconut::bridge`
- `coconut::lua`
- `coconut::webui`
- `coconut::commands`
- `coconut::fs`
- `coconut::error`

Each module should follow the same basic shape where applicable:

- `create(Config *config)`
- `destroy(T *state)`

`Config` is created once and shared across modules.

## Lua and bridge style

- Keep the Lua surface minimal and explicit.
- `coconut.config(ctx)` is the startup config hook.
- `coconut.views()` returns named view descriptors.
- `ctx` is the runtime context object passed into Lua.
- Bridge messages are conceptual object-shaped envelopes, not necessarily JSON internally.
- `emit` is async and queue-based.
- `call` is Promise-based and waits for readiness.

## Tests

- Every functional change should have a test or an updated test plan.
- Prefer small deterministic tests.
- Keep placeholder tests only until the real behavior is implemented.
- The test harness lives under `tests/`.

## Formatting

- Use the repo `.clang-format` file.
- Prefer 2-space indentation.
- Keep includes sorted.
- Keep namespaces uncluttered and readable.

## When in doubt

- Prefer the current spec over inventing new abstractions.
- Prefer minimal state and explicit ownership.
- Ask before making irreversible or ambiguous design changes.
