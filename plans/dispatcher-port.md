# Plan: Add `core::Dispatcher` to `App`, remove `Outbox`

> **Scope:** v0.2 architectural task (parked). NOT part of v0.1.1 hardening.
> **Goal:** Replace the `coconut::dispatch` SPSC `Outbox`-based system with the
> already-existing `coconut::core::Dispatcher` (MessageQueue-based), owned by
> `App`. Delete the `Outbox` class and all `dispatch::Outbox` fields.

---

## 1. Current state (recap)

- `App` (`app.h`) owns `dispatch::Outbox outbox;` (by value) **and**
  `bg_thread::Context* bg` (which holds `inbox`/`outbox` `dispatch::Outbox`).
- `coconut::dispatch` namespace (`dispatch.h`/`dispatch.cpp`): `init`/`shutdown`/
  `notify`/`drain` + `evalJS`/`lifecycleEvent`/`commandCall`. Operates on
  `App.outbox` and `app->bg->outbox`; `drain` reaches directly into `webview_eval`,
  `lua::dispatchViewLifecycleEvent`, and `bridge::rpcSend`. Uses a global
  `g_dispatch_app`.
- `coconut::core::Dispatcher` (`core/dispatcher.h`/`cpp`) — **dead code** today:
  `MessageQueue<DispatchMessage>`, `Dispatcher(App*)`, `queue()`, `flush()`
  (empty). Variant = `{LifecycleMessage, CommandCallMessage, EvalJSMessage}`
  (a `CommandResultMessage` struct is declared but NOT in the variant).
- `bridge` calls `webview_eval`/`rpcSend` **directly** (no dispatcher involved).

## 2. Bridge clarification (for context)

The bridge is the **JS↔Lua transport + router** for both `call` and `emit`,
all four directions. Semantics live in Lua (`coconut._dispatch`, command
registry); the bridge is the pipe. Today the bridge bypasses the dispatcher on
outbound — that overlap is what this port removes.

---

## 3. Phased plan

### Phase 0 — Lock the queue + message model
- Keep `core::Dispatcher` on `MessageQueue` (MPMC, unbounded, **no silent drops**).
- **Add `CommandResultMessage` to the `DispatchMessage` variant** so the single
  dispatcher also carries bg→main results → this lets us **merge `App.outbox`
  + `bg.outbox` into one queue**.
- "Configurable outbox queue capacity" (v0.1.1 item) becomes moot (unbounded).
  Add an optional bound later only if a stall-risk is observed.

### Phase 1 — Add `core::Dispatcher` to `App`
- `app.h`:
  - forward-declare `namespace coconut::core { class Dispatcher; }`
  - add `core::Dispatcher* dispatcher = nullptr;` (raw pointer — consistent with
    sibling submodule pointers `bridge_state`, `bg`, `lua_state`; freed manually
    in `app::destroy`). Include `core/dispatcher.h` (include cycle with
    `../app.h` is already broken by include guards).
- `core/dispatcher.h`:
  - add `CommandResultMessage` to `DispatchMessage`.
  - add `init(App*)`, `shutdown(App*)`, `notify(App*)`; make `flush()` use the
    stored `_App` to reach `webview`/`lua_state`/`bridge_state`.
  - **Move the CFRunLoopSource (macOS) registration out of `dispatch::init` into
    `core::Dispatcher::init`**; drop the global `g_dispatch_app`, use `_App` in
    the perform callback.
  - define `~Dispatcher() = default;` (raw-pointer ownership, manual delete).
- `app::create`: after submodules exist →
  `app->dispatcher = new core::Dispatcher(app); app->dispatcher->init(app);`
- `app::destroy`: `app->dispatcher->shutdown(app); delete app->dispatcher;
  app->dispatcher = nullptr;`

### Phase 2 — Implement `flush()` consumer routing (the bridge split)
`core::Dispatcher::flush()` pops each `DispatchMessage` and **delegates** (does
not execute directly):
- `EvalJSMessage`       → `bridge::callJS(_App, fn, payload)` (or `webview_eval`)
- `LifecycleMessage`    → `bridge::dispatchEventToLua(_App, ViewName, {...})`
  — **retire `lua::dispatchViewLifecycleEvent`**, unifying the duplicate
  lifecycle path.
- `CommandCallMessage`  → route to bg/worker (push to `app->bg->commandQueue`,
  or exec on main). Implements the old `drain` TODO stub.
- `CommandResultMessage`→ `bridge::rpcSend(_App, rpcMsg)`
Result: dispatcher = **router**, bridge = **effector** (layered design).

### Phase 3 — Repoint producers (remove `dispatch::*` Outbox calls)
- Grep all `dispatch::evalJS` / `lifecycleEvent` / `commandCall` / `notify` /
  `init` / `shutdown` / `drain`. Replace with
  `app->dispatcher->queue(...)` / `->flush()` / `->notify()` / `init`/`shutdown`.
- Add thin free helpers (`core::dispatch_evalJS(app, js)` wrapping
  `app->dispatcher->queue(EvalJSMessage{js})`) to minimize caller churn.
- **Bridge outbound decision:** route `bridge::emitToJS`/`callJS` through
  `app->dispatcher->queue(EvalJSMessage{...})` so there is ONE cross-thread path;
  `flush()` performs the actual `webview_eval`. (Removes the current
  bridge-directly-calls-webview overlap.)

### Phase 4 — Repoint `bg_thread`
- `bg_runtime.h`: replace `dispatch::Outbox inbox;` / `outbox;` with a
  `MessageQueue` for **inbound commands** (e.g.
  `std::shared_ptr<MessageQueue<CommandCallMessage>> commandQueue;`) and
  **delete the outbox** — bg results now go to
  `app->dispatcher->queue(CommandResultMessage{...})`.
- `bg_runtime.cpp`: producer pushes `CommandResultMessage` to `app->dispatcher`
  (not `bg->outbox`); consumer pops `CommandCallMessage` from `commandQueue`.
- `core::Dispatcher::flush()` now reads ONLY `app->dispatcher`'s queue — the
  separate `app->bg->outbox` read in the old `drain` disappears. **Two queues
  become one.**

### Phase 5 — Delete `Outbox` & `coconut::dispatch`
- Remove `Outbox` class + `MessageKind`/`Message` from `dispatch.h`.
- Remove `dispatch::init/drain/notify/evalJS/lifecycleEvent/commandCall`
  (delete after all callers repointed; keep thin deprecated wrappers only during
  the transition if needed).
- Remove `App.outbox` field; remove `bg.inbox`/`bg.outbox`.
- Delete/rename `dispatch.h`/`dispatch.cpp`; drop `#include "dispatch.h"` where
  unused; ensure `core/dispatcher.h` is included.
- `bg_runtime.h` include `dispatch.h` → `core/dispatcher.h` (or
  `core/message_queue.h`).

### Phase 6 — Tests
- Update `dispatch_test.cpp`, `dispatch_drain_test.cpp`,
  `dispatch_integration_test.cpp`, `bridge_dispatch_debug_test.cpp`,
  `bg_runtime_test.cpp` to the new `core::Dispatcher` API.
- (These are v0.2 tests; separate from the v0.1.1 "test coverage for
  webview_transport / hotreload / context / argparse / window" item.)

---

## 4. Risks / decisions to lock

| Risk | Note |
|---|---|
| **SPSC → MPMC perf** | bg ring loses lock-free guarantee; mutex per op. Negligible at UI-event rates. Acceptable. |
| **Ordering** | One merged queue → cross-producer interleaving (was separate queues). Usually fine; flag any ordering dependency. |
| **No silent drops** | Unbounded `MessageQueue` removes the `Outbox` drop hazard (good for hardening) but can grow if consumer stalls. Optional bound later. |
| **Include cycle** | `app.h` ↔ `core/dispatcher.h` handled by include guards; verify no ODR issues. |
| **Worker vs bg duplication** | This plan does **NOT** merge `core::WorkerPool` and `bg_thread::Context` — they remain two executors. Outbox removal only unifies the *dispatch/transport* queue, not the executor duplication (separate future task). |
| **Global removal** | Drop `g_dispatch_app`; use `_App` in the CFRunLoopSource perform callback. |

---

## 5. Verification
- Build under ASAN + TSAN (v0.1.1 CI config is useful here for the thread-model
  change).
- Run updated dispatch / integration / bg_runtime tests.
- Manual smoke:
  - JS `coconut.call` → Lua command → result returned.
  - Lua `ctx:emit` → JS receives event.
  - bg command → result reaches JS via `bridge::rpcSend`.
  - lifecycle events reach Lua **once** (no double dispatch after retiring
    `lua::dispatchViewLifecycleEvent`).
