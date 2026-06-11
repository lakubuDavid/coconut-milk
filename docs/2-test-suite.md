# Coconut Milk Test Suite Plan

This document defines the test strategy for Coconut Milk based on the roadmap in `docs/1-roadmap.md`.

The goal is to make the runtime, bridge, Lua integration, command generation, and sample app verifiable in small layers.

---

## 1. Testing goals

Coconut Milk is a bridge-heavy desktop framework, so the test suite should prove these qualities:

- the runtime boots in the right order
- Lua API functions are exposed correctly
- bridge messages are encoded and decoded consistently
- commands bind once and only once
- generated glue follows the spec
- view and asset roots resolve predictably
- frontend promise-based APIs behave correctly
- errors are structured and readable
- startup behavior is deterministic

---

## 2. Test philosophy

### Keep tests layered

Use different test levels for different risk areas:

1. **Unit tests** for pure helpers and data transformation.
2. **Integration tests** for C++ + Lua + generated glue interactions.
3. **Bridge tests** for message routing and promise behavior.
4. **Transport conformance tests** — both legacy and native webview transports
   must satisfy the transport::Transport interface.
5. **End-to-end tests** for the sample application.

### Prefer deterministic tests

The framework should avoid flaky tests by using:

- mockable webview transport wrappers
- fake bridge endpoints
- temporary test directories
- minimal external dependencies

### Transport-layer testing

The transport interface (transport::Transport) is an abstraction that both
Both native and embedded webview transports conform to. Tests should verify:

- send(RpcMessage) dispatches the correct platform call
- setMessageCallback registers and fires on inbound messages
- Inbound native webview events are translated to the correct RpcMessage

### Test the contract, not implementation details

The important thing is whether Coconut behaves according to the spec:

- the right view loads
- the right command is called
- the right payload reaches the right side
- the right error is returned

---

## 3. Test categories

## 3.1 Unit tests

These tests target pure logic or logic that can be isolated from the webview and Lua runtimes.

### Areas

- config defaults
- path normalization
- asset/view resolution
- command-name extraction from annotations
- generated-name formatting
- error formatting
- wire envelope encode/decode helpers
- queue behavior data structures

### Examples

- `views/note.html` resolves to the configured view root
- `assets/logo.png` resolves to the asset root
- duplicate command names are rejected by the registry
- malformed message envelopes are rejected

---

## 3.2 Integration tests

These tests run pieces of the system together.

### Areas

- Lua state initialization
- `coconut.views()` loading
- `coconut.config(ctx)` execution
- `ctx:bind(...)` registration
- `ctx:emit(...)` queueing
- `ctx:call(...)` request/response flow
- generated `.g.lua` loading

### Examples

- Lua returns a view table and the runtime stores it
- a command file registers one or more commands from generated glue
- the frontend helper resolves a Promise with the Lua return value

---

## 3.3 End-to-end tests

These validate the sample application with as much real runtime behavior as possible.

### Areas

- app startup
- initial view loading
- command invocation from frontend
- event emission from Lua to frontend
- ready handshake
- resize hook behavior
- window close behavior

### Examples

- the sample app starts and opens the initial view
- `coconut.ready()` resolves before calls are made
- `coconut.call("hello", ...)` reaches Lua and returns the expected result
- `ctx:emit("greeted", ...)` is received by the frontend listener

---

## 4. Proposed test layout

A practical structure could be:

```text
tests/
  unit/
    config.test.cpp
    path.test.cpp
    registry.test.cpp
    envelope.test.cpp
  integration/
    lua_bootstrap.test.cpp
    command_binding.test.cpp
    bridge_roundtrip.test.cpp
    generated_glue.test.cpp
  e2e/
    sample_app.test.cpp
    startup_flow.test.cpp
    event_flow.test.cpp
  fixtures/
    lua/
    commands/
    views/
    assets/
```

---

## 5. Test harness requirements

### C++ test runner

Coconut should use a native C++ test runner for unit and integration tests.

Recommended properties:

- easy to add to xmake
- supports assertions and subtests
- can run in CI
- can print good failure messages

### Lua test support

Lua-side tests can be run in two ways:

1. from C++ integration tests that initialize Lua directly
2. from a small test Lua harness that is loaded by the C++ test runner

### Frontend test support

Frontend tests can later use a browser automation tool, but for v1 they can also be simulated by a mock bridge endpoint.

---

## 6. Core test matrix

## 6.1 Config and startup

### What to test

- default config values
- custom browser mode selection
- window size propagation
- initial view selection
- startup order
- startup failure reporting

### Cases

- empty config uses defaults
- invalid view name fails clearly
- invalid browser backend fails clearly
- config application is chain-safe

### Expected results

- config fields are applied exactly once
- boot order matches the roadmap
- errors are structured

---

## 6.2 View system

### What to test

- `View.url(...)`
- `View.html(...)`
- `View.load(...)`
- `coconut.views()` registration
- named view lookup
- route switching by name
- asset root resolution

### Cases

- URL views preserve the exact URL string
- HTML views preserve the exact HTML string
- `View.load("note.html")` resolves from the configured view root
- unknown view names fail cleanly
- duplicate view keys are rejected or overwritten according to the final runtime rule

### Expected results

- views remain descriptors until loaded by runtime
- the active view is selected by name

---

## 6.3 Command registry

### What to test

- duplicate command detection
- missing handler rejection
- handler lookup by name
- one-name-one-handler rule
- command name union generation later

### Cases

- binding `hello` twice returns an error
- binding invalid values fails fast
- unknown command call returns a command-not-found error
- a multi-command file registers all exported commands

### Expected results

- registry contains the exact bound commands
- duplicates are prevented

---

## 6.4 Bridge protocol

### What to test

- envelope shapes
- message IDs
- event queueing before ready
- call waiting before ready
- ready handshake
- promise resolution/rejection behavior
- error normalization

### Cases

- `call` envelope has `type`, `id`, `name`, `payload`
- `return` envelope matches the original `id`
- `error` envelope matches the original `id`
- `event` envelope has `name` and `payload`
- `ready` message marks the bridge active
- pre-ready `emit` queues correctly
- pre-ready `call` waits correctly

### Expected results

- protocol is deterministic
- calls and returns are correlated by ID
- errors reject the corresponding Promise

---

## 6.5 Lua API surface

### What to test

- `coconut.views()` loading
- `coconut.config(ctx)` execution
- `coconut.on_resize(...)`
- `ctx:setBrowser(...)`
- `ctx:setWindowSize(...)`
- `ctx:setInitialView(...)`
- `ctx:show(...)`
- `ctx:bind(...)`
- `ctx:emit(...)`
- `ctx:emit_sync(...)`

### Cases

- methods exist and are callable
- chainable methods return the same `ctx`
- action methods behave according to the spec
- `emit` uses table payloads only
- `bind` passes table payloads into Lua

### Expected results

- Lua scripts can control the runtime
- bridge methods are exposed consistently

---

## 6.6 Generated command glue

### What to test

- `.g.lua` generation from annotations
- helper generation for frontend
- single-command files
- multi-command files
- partial annotation fallback
- invalid annotation handling

### Cases

- `---@command hello` generates a binding for `hello`
- a file with `hello` and `goodbye` generates both bindings
- missing `---@return` defaults to `any`
- missing `---@param` defaults to `any`
- function signature multiline parsing works
- LuaDoc tags are respected

### Expected results

- generated binding glue imports the source module
- generated helper code calls `coconut.call(...)`
- command name unions contain discovered commands

---

## 6.7 Asset resolution

### What to test

- `views/` default root
- `assets/` default root
- path normalization
- relative path resolution
- missing file behavior

### Cases

- `note.html` resolves under the view root
- `logo.png` resolves under the asset root
- `../` path traversal is rejected or normalized safely
- missing files produce readable errors

### Expected results

- local file loading is predictable and safe

---

## 6.8 Error handling

### What to test

- runtime error struct formatting
- bridge error formatting
- Lua error conversion
- command-not-found errors
- duplicate binding errors
- file-not-found errors

### Cases

- bad payload produces a structured error
- frontend promise rejection contains code and message
- Lua exceptions become Coconut errors

### Expected results

- errors are never ambiguous
- debug output is actionable

---

## 7. Fixture strategy

### Lua fixtures

Use small, focused Lua files for each behavior:

- simple one-command file
- multi-command file
- malformed annotations
- no annotations
- mixed annotations and missing types

### View fixtures

Use minimal HTML files:

- plain HTML page
- page with bridge listener
- page that emits frontend events

### Asset fixtures

Use tiny test assets:

- image
- text file
- nested path file

### Runtime fixtures

Use fake or temporary runtime roots to isolate tests from the repo layout.

---

## 8. Suggested assertions per phase

## Phase 0: repository skeleton

- headers compile
- xmake target builds
- include paths are correct

## Phase 1: bootstrap

- app creation succeeds
- app destruction is safe
- startup failures are reported

## Phase 2: Lua loading

- Lua state initializes
- Coconut API exists in Lua
- `coconut.views()` and `coconut.config(ctx)` can be called

## Phase 3: views

- view descriptors are created correctly
- named views register correctly
- initial view can be resolved

## Phase 4: Window / Transport

### WebUI transport (current)

- window creates successfully (webui_new_window)
- initial view loads into the window
- resize events are visible
- webui_bind registers __coconut_emit and __coconut_call
- webui_run delivers JS to the page
- webui_wait blocks until window closes

### Webview transport (target)

- webview_create allocates a webview instance successfully
- webview_set_html loads inline HTML
- webview_navigate loads a URL
- webview_init injects Coconut JS runtime before page load
- webview_bind registers __coconut_rpc
- webview_eval delivers JS to the page
- webview_run blocks until window closes
- webview_destroy cleans up without leaking

### Transport interface conformance

Both transports must satisfy transport::Transport:

- send(rpc::Message) delivers the correct JS to the page
  - Type::kEvent → __coconut_dispatch_event(name, payloadJson)
  - Type::kReturn → __coconut_rpc_receive(json)
  - Type::kError  → __coconut_rpc_receive(json)
  - Type::kCall   → globalThis[name](json)
  - Type::kReady  → __coconut_bridge_ready()
- setMessageCallback registers a callback that fires on inbound messages
- Inbound messages are deserialised into rpc::Message

## Phase 5: bridge

- messages are encoded/decoded correctly
- ready state works
- queueing works

## Phase 6: frontend API

- `ready()` resolves
- `call()` resolves or rejects
- `emit()` resolves or rejects
- `on()` returns unsubscribe

## Phase 7: commands

- commands bind once
- duplicates fail
- invocation reaches Lua

## Phase 8: preprocessing

- annotations are detected
- multiline signatures are recognized
- command name extraction works
- generated files are produced

## Phase 9: files

- roots resolve correctly
- missing files fail clearly

## Phase 10: errors

- all major failure paths are structured

## Phase 11: sample app

- sample boots
- sample renders
- sample commands work
- sample events work

## Phase 12: hardening

- edge cases stay stable
- regressions are covered by tests

---

## 9. Priorities

### Highest priority

1. startup
2. Lua loading
3. command binding
4. bridge call/return behavior
5. view resolution

### Medium priority

6. generated glue
7. frontend listener behavior
8. asset resolution

### Lower priority for first pass

9. advanced diagnostics
10. performance edge cases
11. packaging scenarios

---

## 10. Success criteria

The test suite is good enough when it can answer these questions with confidence:

- did the app start?
- did Lua initialize correctly?
- did the initial view load?
- did the command bind?
- did the frontend call succeed?
- did the event get delivered?
- did the queue flush at the right time?
- did failures become structured errors?

---

## 11. Next implementation step

After this plan, the next logical step is to create the actual test harness and add the first tests for:

- `Config`
- path resolution
- command registry
- wire envelope helpers
- Lua bootstrap
