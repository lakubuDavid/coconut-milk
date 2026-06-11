# Coconut Milk Roadmap

This document turns the spec into an implementation plan.
It follows the current design decisions:

- single-window first
- `views/` and `commands/` as the main project roots
- LuaLS-style annotations plus Coconut `---@command`
- generated `.g.lua`, `.d.ts`, and frontend helpers
- Native webview as the browser bridge (WKWebView / WebView2 / WebKitGTK)
- sol2 as the C++ ↔ Lua binding layer
- hybrid bridge protocol with `ready`, `call`, `return`, `event`, and `error`

---

## Phase 0: repository and project skeleton

### Goal
Establish the project layout and the first stable entry points.

### Steps
1. Create the root runtime structure in `src/`.
2. Keep `main.cpp` as a thin bootstrap file.
3. Add the core documentation files:
   - `docs/specs.md`
   - `docs/roadmap.md`
4. Add sample Lua app files and sample commands.
5. Keep the build config (`xmake.lua`) minimal but ready for native webview and Lua.

### Deliverables
- clear folder layout
- buildable binary skeleton
- sample app files for reference

---

## Phase 1: core runtime bootstrap

### Goal
Boot the native application and create the runtime owner objects.

### Steps
1. Create the `App` object.
2. Create the runtime `Config` object.
3. Wire the startup path:
   - parse config
   - create app
   - start runtime
4. Add basic error reporting for boot failures.
5. Keep lifecycle logging simple and visible.

### Deliverables
- `main()` starts the app
- runtime owns config and lifecycle
- startup failures are reported cleanly

---

## Phase 2: Lua runtime loading

### Goal
Load Lua and expose the Coconut API surface to scripts using sol2.

### Steps
1. Create the Lua state wrapper backed by sol2.
2. Expose the `coconut` global/module through sol2 bindings.
3. Register the first Lua-facing hooks:
   - `coconut.views()`
   - `coconut.config(ctx)`
4. Add `ctx` method bindings through sol2:
   - `setWindowSize`
   - `setInitialView`
   - `show`
   - `bind`
   - `emit`
   - `emit_sync`
5. Add Lua error propagation back to C++ through sol2 error handling.

### Deliverables
- Lua scripts can be loaded
- `coconut.views()` and `coconut.config(ctx)` are callable
- `ctx` exists as a runtime object
- sol2 is the binding layer for Lua integration

---

## Phase 3: view system

### Goal
Load and resolve named views from Lua.

### Steps
1. Implement the `View` module in Lua/C++ terms.
2. Support the three view factory types:
   - `View.url(...)`
   - `View.html(...)`
   - `View.load(...)`
3. Add default root resolution for `views/` and asset root resolution for `assets/`.
4. Store named views returned by `coconut.views()`.
5. Add `setInitialView(name)` routing.
6. Add `show(name)` view switching.

### Deliverables
- named views can be registered
- startup can open the initial view
- runtime can switch views by name

---

## Phase 4: native window integration

### Goal
Create the native desktop window and connect it to the view system.

### Steps
1. Wrap native webview window creation.
2. Set browser backend mode from config.
3. Apply initial window size.
4. Load the selected initial view.
5. Add close and resize handling.
6. Keep window operations in one backend module.

### Deliverables
- app window opens successfully
- initial view renders in native webview
- basic resize/close behavior works

---

## Phase 5: bridge protocol

### Goal
Define the Coconut message model across JS, C++, and Lua.

### Steps
1. Implement the conceptual wire envelope:
   - `ready`
   - `call`
   - `return`
   - `event`
   - `error`
2. Add message correlation IDs for `call` / `return` / `error`.
3. Add queueing for `emit(...)` before readiness.
4. Add waiting behavior for `call(...)` before readiness.
5. Add structured bridge errors.
6. Add the frontend `ready` handshake.

### Deliverables
- frontend and Lua traffic share one protocol shape
- async events queue before ready
- calls wait until ready
- errors are normalized

---

## Phase 6: frontend JavaScript API

### Goal
Expose the frontend runtime API to the webview.

### Steps
1. Add `coconut.ready()`.
2. Add `coconut.call(name, payload)`.
3. Add `coconut.emit(name, payload)`.
4. Add `coconut.on(name, fn)`.
5. Make `coconut.on(...)` return an unsubscribe function.
6. Ensure `emit(...)` returns `Promise<void>`.
7. Ensure `call(...)` resolves with the Lua return value.

### Deliverables
- frontend code can call Lua commands
- frontend code can listen for Lua events
- frontend code can emit app-level events

---

## Phase 7: command registry and binding

### Goal
Store and resolve command handlers on the Lua side.

### Steps
1. Create `CommandRegistry`.
2. Enforce one handler per command name.
3. Bind command names from `ctx:bind(...)`.
4. Route frontend `call(...)` requests to the bound Lua function.
5. Return values and errors back through the bridge.

### Deliverables
- command names are registered once
- duplicate bindings fail fast
- frontend calls execute Lua handlers

---

## Phase 8: command preprocessing and generation

### Goal
Turn annotated command source files into generated glue and helpers.

### Steps
1. Implement the Coconut preprocessor.
2. Support LuaLS annotations plus `---@command`.
3. Detect command blocks using the basic parser.
4. Read only as much of the signature as needed.
5. Support multiline signatures and `function` / `local function`.
6. Infer fallback types as `any` when incomplete.
7. Generate `.g.lua` bind files.
8. Generate `.d.ts` typing files.
9. Generate frontend helper modules.

### Deliverables
- multi-command source files can be scanned
- generated glue binds every discovered command
- frontend helpers wrap `coconut.call(...)`

---

## Phase 9: assets and file resolution

### Goal
Make local file loading predictable.

### Steps
1. Add asset root resolution.
2. Support `views/` as the default view root.
3. Support `assets/` as the default static file root.
4. Add path normalization and safe resolution.
5. Keep packaged and development layouts compatible.

### Deliverables
- `View.load(...)` works consistently
- static assets resolve from a known root
- path behavior is deterministic

---

## Phase 10: error handling and diagnostics

### Goal
Make failures easy to understand and debug.

### Steps
1. Standardize Coconut error codes.
2. Convert Lua errors into bridge errors.
3. Convert bridge errors into frontend rejections.
4. Add startup diagnostics for missing views, commands, and files.
5. Add duplicate command and invalid payload diagnostics.

### Deliverables
- runtime errors are structured
- command and bridge failures are visible
- debugging output is consistent

---

## Phase 11: sample application and verification

### Goal
Prove the framework with a full minimal sample.

### Steps
1. Build a sample app using `coconut.lua`.
2. Add a `views/` folder with at least one HTML view.
3. Add a `commands/` folder with one single-command file and one multi-command file.
4. Generate the `.g.lua`, `.d.ts`, and helper modules.
5. Verify `call`, `emit`, `on`, and `ready` behavior.
6. Verify startup and view switching.

### Deliverables
- end-to-end sample app
- generated artifacts visible in the repo
- documented flow from source to runtime

---

## Phase 12: hardening and polish

### Goal
Stabilize the first usable release.

### Steps
1. Improve tests around startup and bridge behavior.
2. Add command generation checks.
3. Add edge case handling for partial annotations.
4. Improve packaging and build scripts.
5. Review performance and startup latency.
6. Clean up public API documentation.

### Deliverables
- stable developer experience
- fewer edge case regressions
- release-ready v0/v1 runtime

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
- The bridge should stay conceptual and object-shaped even if the implementation uses native C++/Lua data structures.
- The goal is to keep the first working version minimal and predictable.
