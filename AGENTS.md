# Coconut Milk — Agent Guide

Rules you must follow when writing code or making decisions.

## Versioning scheme

We follow strict **semantic versioning** (`MAJOR.MINOR.PATCH`):

- **MAJOR** (v1.0.0, v2.0.0): Full API change / breaking changes. Backward
  incompatible modifications to the public Lua API, C++ API, bridge protocol,
  or config format.
- **MINOR** (v0.2.0, v0.3.0): New features. Adding functionality without
  breaking existing code. The public API remains backward compatible.
- **PATCH** (v0.1.1, v0.2.1): Performance improvements and bug fixes.
  No API changes of any kind — no new functions, no new parameters,
  no new config fields.

When deciding which version bucket a change belongs in, ask:
"Would existing user code need to change?" If yes → MAJOR.
"Does this add new capability without breaking anything?" If yes → MINOR.
"Is this just making things faster / safer without new surface?" If yes → PATCH.

Current version (defined in `src/argparse.h` as `COCONUT_VERSION`): **0.1.1**

### Branching & tagging

- **`main`** is the single development branch — all PRs target it.
- Releases are marked with **annotated tags** (`git tag -a vX.Y.Z`).
- Experiment branches use the naming convention:
  `v{base-version}/exp/{short-description}`
  (e.g. `v0.1.1/exp/unique-ptr-migration`).
- When the base version advances, experiment branches are rebased or
  deleted — the prefix always tells you what they were forked from.

## Core design decisions

- Single-window first.
- Lua is the application authoring language.
- Native webview is the browser bridge.
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

## Error Handling
- Prefer `std::expected<T, Error>` or `std::optional` for recoverable failures.
- Be defensive, use error as values where possible, try catch where something may fail or might need to bubble up the errors
- Use `ErrorCode` + `Error` as the shared error vocabulary.
- Avoid exceptions for normal control flow.
- Avoid silent failure

## Module layout

Each module should follow the same basic shape where applicable:

- `create(Config *config)`
- `destroy(T *state)`

`Config` is created once and shared across modules.

Current modules:
`coconut::app`, `coconut::bridge`, `coconut::lua`,
`coconut::commands`, `coconut::fs`, `coconut::error`

## Lua and bridge style

- Keep the Lua surface minimal and explicit.
- `coconut.config(ctx)` is the startup config hook.
- `coconut.views()` returns named view descriptors.
- `ctx` is the runtime context object passed into Lua.
- Bridge messages are conceptual object-shaped envelopes.
- `emit` is async and queue-based.
- `call` is Promise-based and waits for readiness.

## Platform Specific Actions
- Platform events/interactions should path trough an "interface/adapter" object/module;
If any module need to interact with platform specific features that call platforms API,
it should call the interface/adapter.
`
  DONT: module -> plaform code
  DONT: (needs to move window) call platform/OS/window.h window_api::move
  DO: module -> interface module -> platform code
  DO: (needs to move window) call window::move -> which calls platforms/OS/window.h window_api::move
`

## When in Doubt

- Prefer the current spec over inventing new abstractions.
- Prefer minimal state and explicit ownership.
- Ask before making irreversible or ambiguous design changes.


