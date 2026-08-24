# thread

## What it is
The threading model: one **main thread** (owns webview, all native APIs, UI)
and **N background worker threads** (run user commands off the main thread),
plus a marshalling system that lets worker code reach main-thread-only
platform APIs.

## Why we use it
- The main thread runs the native event loop (`webview_run` / CFRunLoop) and
  **must never block** — blocking it freezes the UI
- Lua commands route by registry: `mt_handlers` / main `handlers` execute on
  the main thread (Bridge sync executor); everything else goes to workers
- Platform APIs (dialog, notify, clipboard, window) are main-thread-only and
  reached from workers via forwarding ([[Main-thread forwarding pattern]]
  in the vault; implemented as `dispatch::post` + `forwardToMain`)

## Key concepts
- **Main thread**: owns `webview_t`, the dispatch pump (CFRunLoopSource via
  `platform/runloop.h`), the posted-task queue, and runs `Dispatcher::flush()`
- **Worker pool**: `core::WorkerPool` owns N `Worker`s; each has its own
  `sol::state`, its own `CoconutContext` + command map (registries are never
  shared across VMs), fed round-robin through MPSC-safe queues
- **`dispatch::post(fn)`**: push a closure onto the main thread; drained in
  `dispatch::drain()` alongside the dispatcher flush
- **`modules/forward.h → forwardToMain(op)`**: blocking variant — posts op,
  waits on a future. Used by store/dialog/clipboard/notify/openurl/window
  bindings so worker code gets real results
- **Window module**: `coconut.window.*` is registered with a ThreadKind switch
  — direct calls on main, marshalled calls from workers

## How we use it here
- `main.cpp` builds the trio after transport creation: per-worker
  `CoconutContext`s → `WorkerPool::builder(2)` (ThreadSafe|BG_STUBS|WINDOW|
  CLIPBOARD|NOTIFY|OPENURL|DIALOG modules) → `.withOutputNotifier(notify)`
- `set_window_*` builtins are thin wrappers over `coconut.window.*`; identical
  behavior whether invoked from JS (sync executor) or a custom worker command
- Shutdown order matters: dispatcher reset (joins workers) before worker
  contexts/Lua states die; transport last

## Gotchas
- **One registry per VM**: never share a `commands::Registry` or its sol
  functions across Lua states
- **Main never blocks on workers**: only workers block on main (bounded by
  drain cadence). Keep it that way to avoid deadlock cycles
- **No preemption**: a CPU-bound command occupies its worker until it returns;
  size the pool for your workload
- **Lazy target lookup**: module bindings must resolve resources at call time,
  not capture them at registration (startup ordering bites otherwise)
