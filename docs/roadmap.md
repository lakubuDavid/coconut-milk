# Coconut Milk Roadmap

This document tracks the implementation plan for Coconut Milk.

Current design decisions:

- Single-window first
- `views/` and `commands/` as the main project directories
- LuaLS-style annotations plus `---@command` for code generation
- Generated `.g.lua`, `.d.ts`, and frontend helpers
- Native webview as the browser bridge
- Hybrid bridge protocol with `ready`, `call`, `return`, `event`, and `error`

---

## Phase 0: repository and project skeleton

### Goal
Establish the project layout.

### Deliverables
- Clear folder layout
- Buildable binary with `xmake`
- Sample Lua app files and sample commands
- Core documentation (`specs.md`, `roadmap.md`)

---

## Phase 1: core runtime bootstrap

### Goal
Boot the native application.

### Deliverables
- Parse config at startup
- Create the app runtime
- Basic error reporting for boot failures
- Lifecycle logging

---

## Phase 2: Lua runtime loading

### Goal
Load Lua and expose the Coconut API to scripts.

### Deliverables
- Lua scripts can be loaded at startup
- `coconut.views()` and `coconut.config(ctx)` are callable
- `ctx` runtime object with methods:
  - `setWindowSize`, `setInitialView`, `show`
  - `bind`, `emit`, `emit_sync`
- Lua error propagation back through the bridge

---

## Phase 3: view system

### Goal
Load and resolve named views from Lua.

### Deliverables
- Three view factory types: `View.url()`, `View.html()`, `View.load()`
- Root resolution for `views/` and `assets/` directories
- Named views returned by `coconut.views()`
- `setInitialView(name)` routing
- `show(name)` view switching at runtime

---

## Phase 4: native window integration

### Goal
Create the native desktop window and connect it to the view system.

### Deliverables
- App window opens with configured size
- Initial view renders in native webview
- Window resize and close behavior

---

## Phase 5: bridge protocol

### Goal
Define the Coconut message model.

### Deliverables
- RPC envelope types: `ready`, `call`, `return`, `event`, `error`
- Correlation IDs for `call` / `return` / `error` pairing
- Event queueing before bridge readiness
- Calling waits until bridge is ready
- Structured bridge errors with codes

---

## Phase 6: frontend JavaScript API

### Goal
Expose the frontend runtime API to the webview.

### Deliverables
- `coconut.ready()` — wait for bridge
- `coconut.call(name, payload)` — invoke Lua commands
- `coconut.emit(name, payload)` — send events to Lua
- `coconut.on(name, fn)` — listen for Lua events
- `coconut.on()` returns an unsubscribe function
- All methods return Promises

---

## Phase 7: command registry and binding

### Goal
Store and resolve command handlers.

### Deliverables
- One handler per command name
- Duplicate bindings fail fast
- `ctx:bind()` registers commands
- Frontend `call()` dispatches to Lua handlers
- Return values and errors flow through the bridge

---

## Phase 8: command preprocessing and generation

### Goal
Turn annotated command source files into generated glue and helpers.

### Deliverables
- Annotations: `---@command`, `---@param`, `---@return`
- Scans `commands/*.lua` for annotated functions
- Generates `.g.lua` (Lua registration)
- Generates `.d.ts` (TypeScript types)
- Generates `.g.js` (frontend helper wrappers)
- Supports multiline signatures

---

## Phase 9: assets and file resolution

### Goal
Make local file loading predictable.

### Deliverables
- Asset root resolution from project root
- `views/` as default view directory
- `assets/` as default static file directory
- Path normalization and safe resolution

---

## Phase 10: error handling and diagnostics

### Goal
Make failures easy to understand and debug.

### Deliverables
- Standardized error codes (`CommandNotFound`, `NotReady`, `DuplicateCommand`, etc.)
- Lua errors propagated as bridge errors
- Bridge errors become frontend Promise rejections
- Startup diagnostics for missing views, commands, and files

---

## Phase 11: sample application and verification

### Goal
Prove the framework with a full minimal sample.

### Deliverables
- End-to-end sample app with views and commands
- Generated artifacts (`.g.lua`, `.d.ts`, `.g.js`)
- Verified `call`, `emit`, `on`, and `ready` behavior
- Verified startup and view switching

---

## Phase 12: hardening and polish

### Goal
Stabilize the first usable release.

### Deliverables
- Improved test coverage
- Edge case handling for partial annotations
- Packaging and build scripts
- Performance and latency review
- Clean public API documentation

---

## Recommended execution order

If the goal is to ship quickly, do the work in this order:

1. repository skeleton
2. Lua runtime bootstrap
3. view system
4. WebUI window integration
5. bridge protocol
6. frontend API
7. command registry
8. command preprocessing/generation
9. assets and file resolution
10. error handling
11. sample app verification
12. hardening

---

## Notes

- Each phase should be small enough to implement and test independently.
- The command preprocessor can remain intentionally simple in v1.
- The bridge protocol stays conceptual — messages are object-shaped even if the implementation uses native data structures.
- The goal is to keep the first working version minimal and predictable.
