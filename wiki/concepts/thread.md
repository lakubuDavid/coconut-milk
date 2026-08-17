# thread

## What it is
The multi-threading model: one **main thread** (owns webview, platform APIs, UI)
and one **background thread** (runs user commands off the main thread).

## Why we use it
- The main thread runs a native event loop (`webview_run` / `CFRunLoop`) and
  **must never block** — blocking the main thread freezes the UI
- CPU-heavy or I/O-bound Lua commands (`ctx:bind`) run on the background thread
  by default
- Platform APIs (dialog, notify, clipboard, window control) are main-thread-only
  and accessed via `ctx:bind_mt` or the forwarding system

## Key concepts
- **Main thread**: owns `webview_t`, `dispatch::Outbox` (SPSC), CFRunLoopSource,
  all platform adapters
- **Background thread**: owns a separate `sol::state`, `commands::Registry`,
  `CoconutContext` with `is_main_thread = false`
- **Two SPSC queues**: `bg->inbox` (main→bg `CommandCall`) and
  `bg->outbox` (bg→main `CommandResult`)
- **`dispatch::notify(app)`**: signals the CFRunLoopSource so the main thread
  drains queues promptly
- **`lua_yield` / `lua_resume`**: background coroutines suspend on `await()`
  and resume when a `ForwardResult` arrives — the C++ thread is NOT blocked

## How we use it here
- `bg_thread::Context` holds the background state (created but not yet started
  in v0.1.1)
- `dispatch::Outbox` is a lock-free ring buffer (capacity 64) with separate
  producer/consumer atomics — no mutex needed
- `dispatch::drain()` processes both the main outbox (`EvalJS`, `LifecycleEvent`,
  `CommandCall`) and the bg outbox (`CommandResult`) on each run-loop iteration
- `WebviewTransport::resolveBgCommand` maps `callId` → webview callback ID for
  resolving background command results back to JS

## Gotchas
- **SPSC only**: the outbox is single-producer/single-consumer by design.
  Adding more producers requires a different queue (ADR-0004)
- **No preemption**: a CPU-bound command that never `yield()`s will occupy the
  background thread until it returns
- **Main thread never blocks**: all cross-thread calls from main are async
  (futures with `then()`/`catch()`); only bg→main uses `await()`
- **`app->bg` is null in v0.1.1**: the background thread exists in code but is
  not started from `main.cpp` — activation wiring lands in v0.2.0
