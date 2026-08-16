# Memory Safety & Optimization Plan

**Status:** Draft — research & planning only  
**Target:** v0.2.0+ (incremental, not a single refactor)  
**Context:** Another agent may be editing code in this repo concurrently. Do not implement until this plan is reviewed and scoped.

---

## Part 1 — Memory Safety & Crash Prevention

### 1.1 sol3/lua lifetime guards

**Problem:** The pre-existing crash (`lua_rawgeti` with null `lua_State*`) was caused by a default-constructed `sol::table{}` whose internal `lua_State*` was null. sol3 permits this construction but crashes when you try to use the table.

**Plan:**

- [ ] Audit every `sol::table` / `sol::state_view` / `sol::function` for potential default-construction paths.
- [ ] Add a helper wrapper or polyfill that asserts/enforces non-null `lua_State*`:

```cpp
// src/lua_guard.h (new)
namespace coconut::lua {

/// Wraps a sol::table and validates it has a live lua_State* on access.
template<typename T>
class SafeRef {
  sol::table m_ref;
public:
  explicit SafeRef(sol::table ref) : m_ref(std::move(ref)) {}
  T get(const char* key) {
    if (!m_ref.valid()) return T{};
    sol::object obj = m_ref[key];
    if (!obj.valid()) return T{};
    return obj.as<T>();
  }
  // ... operator->, operator* etc with checks
};

} // namespace
```

- [ ] Alternatively, use `sol::table::valid()` before every `operator[]` call on tables that may come from Lua callbacks.
- [ ] Wrap `descriptor["key"]` lookups with `.get_or()` default-value pattern.
- [ ] Consider using `sol::protected_function` with `sol::script_pass_on_error` consistently.

### 1.2 Raw `App*` pointer lifetime

**Problem:** `App` pointers are passed as raw pointers through dispatch callbacks, bridge handlers, and platform delegates. If `App` is destroyed (unlikely now, but possible with hot-reload/window-recreation), any in-flight callback would be a use-after-free.

**Plan:**

- [ ] Introduce a `std::shared_ptr<App>` or a handle-table (integer ID → `App*`) so that dangling references can be detected.
- [ ] Add an `app::alive()` or `is_valid()` flag that is set to false in the `App` destructor and checked in dispatch handlers.
- [ ] At minimum: add `app::isShuttingDown()` guards in all bridge/dispatch callbacks that touch `App`.

### 1.3 ObjC manual memory management

**Problem:** The macOS scheme handler and permission code use `__bridge_retained` / `__bridge_transfer` casts and `objc_allocateClassPair`. Mistakes here cause leaks or crashes.

**Plan:**

- [ ] Audit all `__bridge_retained` / `__bridge_transfer` casts — ensure every `_retained` has a matching `_transfer` or `CFRelease`.
- [ ] Consider moving to a thin ObjC++ wrapper class with `-dealloc` that owns the `WKURLSchemeHandler` and cleans up in its destructor.
- [ ] Document the retain/release contract for each new ObjC object.

### 1.4 Null-deref hardening

**Problem:** Several places dereference pointers without null-check (e.g. `runtime->app->configs` chains, webview handle access).

**Plan:**

- [ ] Add `[[gsl::not_null]]` annotations or equivalent for function parameters that must never be null.
- [ ] For nullable pointers (`App*`, `WebviewTransport*`), add early-return guards at the top of every method that dereferences them:

```cpp
if (!m_app || m_app->isShuttingDown()) return;
```

- [ ] Use `std::optional<T>` for values that may be absent (instead of sentinel values or empty strings).
- [ ] Use `std::expected<Result, Error>` for all fallible operations (already started — extend coverage).

### 1.5 Lock-free SPSC queue correctness

**Problem:** The lock-free outbox in `dispatch.h` is inherently subtle. Wrong memory ordering could cause lost events or consumer/producer desync.

**Plan:**

- [ ] Review all `std::atomic` operations in `dispatch.h` for correctness of memory ordering (most should be `acq_rel` or `release`/`acquire` pairs).
- [ ] Add a stress-test target that runs the dispatch queue under heavy load (multi-million iterations, random sizes).
- [ ] Consider adding a `ThreadSanitizer` CI job (even if just for the dispatch tests).
- [ ] Document the memory-ordering contract at the top of `dispatch.h`.

### 1.6 Webview callback safety

**Problem:** Webview callbacks (`webview_bind` callbacks, scheme handler tasks) can fire asynchronously. If they fire after app shutdown starts, they may access destroyed state.

**Plan:**

- [ ] Add a `std::atomic<bool> g_app_is_shutting_down` flag set early in `main.cpp`'s shutdown path.
- [ ] Check this flag at the top of every webview callback, bridge handler, and platform delegate.
- [ ] For macOS scheme handler: handle the case where `WKURLSchemeTask` has already been cancelled (add a `stopURLSchemeTask` implementation that actually flags the task as invalid).

### 1.7 C++ exception policy

**Problem:** The codebase uses a mix of exceptions (`throw std::runtime_error`) and error codes. Exceptions across the Lua boundary (sol3) or webview callbacks cause undefined behaviour.

**Plan:**

- [ ] Establish a guideline: **no exceptions across module boundaries** (Lua, webview, platform callbacks). Use `std::expected` or error enums instead.
- [ ] Wrap all Lua callback invocations with `sol::protected_function` (which catches exceptions).
- [ ] Wrap all webview `webview_bind` callbacks with try/catch that logs and returns an error envelope instead of crashing.
- [ ] Audit current `throw` sites and decide which should become `Error` returns.

---

## Part 2 — Error Handling Improvements

### 2.1 Consistent `std::expected` usage

**Problem:** The project has `coconut::Error` and `ErrorCode` but not all fallible functions use them. Some return `bool` or throw.

**Plan:**

- [ ] Define a project-wide alias:

```cpp
// src/expected.h (new, tiny)
#include <expected>
#include "error.h"

namespace coconut {
template<typename T>
using Result = std::expected<T, Error>;
}
```

- [ ] Migrate functions that currently return `bool` (with side-channel error) to return `Result<T>`:

| Current pattern | Future pattern |
|---|---|
| `bool readFile(path, &outData)` | `Result<std::vector<uint8_t>> readFile(path)` |
| `int writeFile(path, data)` (0=ok) | `Result<void> writeFile(path, data)` |
| `bool tryX()` (must check log for why) | `Result<void> tryX()` |

- [ ] Make `Error` printable / formattable (`std::formatter<Error>`) so it plugs into `debug::*` cleanly.

### 2.2 Lua error boundaries

**Problem:** Lua scripts can throw errors that propagate through sol3 into C++. Currently not all call sites guard with `pcall` or `sol::protected_function`.

**Plan:**

- [ ] Create a `lua::pcall()` wrapper that:
  - Catches all Lua errors
  - Logs them via `debug::error` with full Lua stack trace
  - Returns a default value or re-raises as a C++ `Error`
- [ ] Ensure all `ctx:bind()` handlers are wrapped with `sol::protected_function` (currently they use `sol::protected_function` for the stored fn — verify).
- [ ] Add Lua stack trace extraction to error messages (sol3 provides `sol::error::what()` but not always the full trace).

### 2.3 Structured error reporting to JS

**Problem:** Errors from Lua commands reach JS as ad-hoc strings or `{ok:false, error:{...}}` objects, but the shape isn't guaranteed to be consistent.

**Plan:**

- [ ] Define a canonical JS error envelope shape in `schemas/coconut.d.ts`:

```ts
interface BridgeError {
  code: string;
  message: string;
  details?: unknown;
  stack?: string;        // Lua stack trace (if available)
  command?: string;      // command name that failed
  timestamp?: number;    // millis since epoch
}
```

- [ ] Populate as many fields as possible on the C++ side before serialising.

### 2.4 Assertion policy

**Problem:** No consistent assertion strategy. Some places use `assert()`, some throw, some silently ignore preconditions.

**Plan:**

- [ ] Define a `COCONUT_ASSERT(cond, msg)` macro that:
  - In debug builds: `assert()` + log
  - In release builds: log + return `Error` or early-return (does not abort)
- [ ] Do NOT use `assert()` for conditions that could be triggered by malformed input (Lua scripts, config files). Those must always be handled gracefully.
- [ ] Reserve `assert()` for internal invariant violations (should-never-happen state corruption).

---

## Part 3 — Bundle Size Optimisation

### 3.1 Strip unused code

**Plan:**

- [ ] Run `include-what-you-use` (IWYU) on all translation units to find unused `#include` directives.
- [ ] Remove dead template instantiations (especially in webview headers which are heavy).
- [ ] Audit `sol2` include footprint — it's a large header-only library. Consider forward-declaring and `#include`-ing only what's needed in each `.cpp`.
- [ ] Consider compiling sol2 as a single translation unit (precompiled header or unity build) to avoid re-instantiation across many `.cpp` files.

### 3.2 Link-time optimisation tuning

**Problem:** LTO is already enabled in release builds but may be causing the pre-existing SIGBUS crash (`realizeClassWithoutSwift` writing to `__TEXT`).

**Plan:**

- [ ] Experiment with `-flto=thin` vs `-flto=full` on macOS (ThinLTO usually avoids the ObjC class weirdness).
- [ ] Add explicit `__attribute__((used))` to ObjC classes and protocols that LTO might strip.
- [ ] Consider splitting the release build into: `-flto=thin` for most code, no LTO for `.mm` (ObjC++) files.

### 3.3 Dependency audit

**Plan:**

- [ ] List all third-party dependencies and their actual usage:

| Library | Purpose | Bundle impact | Replaceable? |
|---|---|---|---|
| luajit | Lua runtime | ~500KB | No |
| sol2 | Lua binding | ~300KB header | Maybe (custom thin binding?) |
| nlohmann_json | JSON parsing | ~50KB header | Maybe (simdjson?) |
| lunasvg | SVG rendering | ~100KB | Maybe (just for icons?) |
| webview | Webview engine | ~30KB header | No (but it's huge template code) |

- [ ] For sol2: evaluate if a hand-rolled minimal binding for the ~20 functions we actually use would be smaller and safer.
- [ ] For nlohmann_json: it's already small; keep it.
- [ ] For lunasvg: if only used for icon generation, consider pre-rendering icons to PNG at build time and removing the runtime dependency.
- [ ] For webview: audit whether we need the full header chain or can use forward declarations.

### 3.4 Compiler/linker flags

**Plan:**

- [ ] Add to release mode:
  - `-Wl,-S` — strip debug symbols (xmake should do this already)
  - `-fvisibility=hidden` — hide internal symbols, smaller binary, better optimisation
  - `-fdata-sections -ffunction-sections -Wl,--gc-sections` — drop unreferenced sections
  - `-Oz` instead of `-O2`/`-O3` on macOS (optimise for size)
- [ ] Measure before/after with `size` and `du -h` on the binary.
- [ ] Create a CI job that reports binary size deltas on pull requests.

---

## Part 4 — Runtime Performance

### 4.1 Memory allocation patterns

**Problem:** The dispatch queue, bridge messages, and file reads allocate and free frequently. No awareness of allocation hot spots.

**Plan:**

- [ ] Profile with Instruments (macOS) / `perf` (Linux) to find allocation-heavy paths.
- [ ] For the dispatch queue: use a fixed-size ring buffer (already SPSC — ensure it's lock-free with a static pool, not dynamically growing).
- [ ] For bridge RPC messages: consider a small-object allocator or a bump-allocator for the typical message size (~200-500 bytes).
- [ ] For `routes::handle()` file reads: add a small LRU cache for frequently requested assets (CSS, JS bundles).
- [ ] Ensure `std::string` is not copied unnecessarily: pass `std::string_view` in hot paths, use `std::move` at API boundaries.

### 4.2 Lua GC tuning

**Problem:** LuaJIT's garbage collector runs periodically. On a desktop app with smooth 60fps rendering, GC pauses can cause visible stutter.

**Plan:**

- [ ] Tune `lua_gc` parameters at startup:
  - Set GC pause ratio higher (e.g. 200 instead of default 100) — less frequent collections.
  - Set GC step multiplier higher (e.g. 200) — faster incremental steps.
- [ ] Measure GC pause times with `lua_gc(LUA_GCCOUNT)` before/after actions.
- [ ] Consider `lua_gc(LUA_GCSTEP, ...)` in idle moments (e.g. after view switch, during dispatch drain) to spread GC work.

### 4.3 Dispatch queue throughput

**Plan:**

- [ ] Benchmark the SPSC queue at different load levels: events per second, consumer lag, cache-line bouncing.
- [ ] Ensure the producer/consumer are on separate cache lines (align the ring buffer to `alignas(std::hardware_destructive_interference_size)`).
- [ ] Batch drain: `dispatch::drain()` already processes all pending items. Ensure it doesn't yield/return between items unnecessarily.

### 4.4 Webview RPC batching

**Problem:** Every `coconut.call()` or `coconut.emit()` from JS triggers a `webview_eval()` call. On macOS each eval crosses the ObjC bridge → WKWebView message loop — high latency per call.

**Plan:**

- [ ] Batch multiple RPC messages into a single `webview_eval()` call (e.g. `__coconut_batch([msg1, msg2, ...])`).
- [ ] Flush the batch on a timer or when the batch reaches a size threshold.
- [ ] Measure round-trip latency before/after.

### 4.5 Startup time

**Plan:**

- [ ] Profile the startup sequence (main → app::create → config load → Lua init → webview create → first view load).
- [ ] Defer non-critical initialisation:
  - Load Lua command files lazily (not at startup, but on first call).
  - Defer scheme handler registration until first `coconut://` request? (may not be possible on macOS — must be before webview create).
  - Init icon generation lazily.
- [ ] Parallelise: config parsing, Lua state creation, and webview creation are currently serial. Some can overlap.

### 4.6 Profiling instrumentation

**Plan:**

- [ ] Add lightweight tracing macros (`COCONUT_TRACE(name)`) that log enter/exit with timestamps.
- [ ] Guard with `#if !defined(NDEBUG)` or a `COCONUT_PROFILING` define.
- [ ] Output to Chrome trace format (`chrome://tracing`) for visual analysis.

---

## Part 5 — Measurement & Validation

### 5.1 Metrics to track

| Metric | Current (approx) | Target | How to measure |
|---|---|---|---|
| Binary size (release) | ~2-5MB? | <3MB | `du -h build/*/release/coconut` |
| Startup to first frame | ~500ms? | <200ms | `time coconut` or Instruments |
| Bridge call latency (p50) | ? | <5ms | Instrument `coconut.call("ping")` |
| Dispatch throughput | ? | >1M events/s | Micro-benchmark |
| Lua GC pause (p99) | ? | <8ms | `lua_gc(LUA_GCCOUNT)` hooks |

### 5.2 CI integration

- [ ] Add a binary-size CI check that warns on >5% increase.
- [ ] Add a basic benchmark CI job that runs `coconut --version` + a ping latency test and reports regressions.
- [ ] Optional: add ThreadSanitizer job for dispatch tests.

---

## Execution order (recommended)

| Priority | Task | Effort | Risk |
|---|---|---|---|
| **P0** | Add `App::isShuttingDown` guard + null checks in all callbacks | 1 day | Low — trivially safe |
| **P0** | Wrap all sol::table lookups with `.valid()` or `.get_or()` | 0.5 day | Low — proven crash fix |
| **P1** | Establish `coconut::Result<T>` alias and migrate file/bridge functions | 2 days | Medium — touches many files |
| **P1** | Audit ObjC retain/release balance | 0.5 day | Low |
| **P1** | LTO flags for release (ThinLTO, -gc-sections, -fvisibility) | 0.5 day | Low — config change |
| **P1** | Lua GC tuning at startup | 0.5 day | Low |
| **P2** | Include-what-you-use audit | 1 day | Low — mechanical |
| **P2** | Dispatch queue memory ordering review + stress test | 1 day | Medium — subtle |
| **P2** | Bundle size CI check | 0.5 day | Low |
| **P3** | RPC batching | 2 days | Medium — perf optimisation |
| **P3** | LRU asset cache in scheme handler | 1 day | Low |
| **P3** | sol2 → thin custom binding evaluation | 3 days | High — research first |

---

## Non-goals

The following are explicitly **out of scope** for this plan:

- Replacing LuaJIT with another runtime (too fundamental)
- Rewriting the webview library (too large)
- Adding Rust/C FFI for memory safety (interesting but not practical now)
- Full codebase `gsl::not_null` annotation (too noisy; use judiciously)
