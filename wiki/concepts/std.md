# std

## What it is
C++20/23 standard library features used throughout the codebase.

## Why we use it
- `std::expected<T, E>` — typed error handling without exceptions
- `std::optional<T>` — nullable value semantics
- `std::format` — type-safe string formatting (C++20)
- `std::unordered_map` / `std::map` — command registry, view lookup
- `std::atomic` + `alignas(64)` — lock-free SPSC queue (dispatch outbox)
- `std::thread` — background command thread (v0.2.0)
- `std::filesystem` — path resolution, file existence checks

## Key concepts
- **`std::expected<bool, Error>`** — the standard return type for fallible
  operations across the codebase (`app::create`, `lua::create`, `loadEntryPoint`, …)
- **`std::expected` chaining**: `if (!result) { return std::unexpected(result.error()); }`
- **Error vocabulary**: `struct Error { ErrorCode code; std::string message; std::string details; }`

## How we use it here
- Every `create()` factory returns `std::expected<T*, Error>`
- `std::optional<Error>` is used where "no error" is a valid state
  (`app::getError`)
- `std::format("key='{}'", value)` replaces `sprintf` / `stringstream`
- `alignas(64)` on `write_idx_` / `read_idx_` prevents false sharing in the
  lock-free `dispatch::Outbox` ring buffer

## Gotchas
- **C++26 on macOS**: `set_languages("c23", "c++26")` makes enum-to-int
  arithmetic a hard error (not suppressible). `.mm` files are compiled with
  `-std=c++23` to avoid this
- **`std::expected` is C++23 on non-macOS**: macOS gets C++26, others get C++23
  — both support `std::expected` but the standard version differs
- **`std::filesystem::current_path` throws**: all directory changes are wrapped
  in `try/catch` with `debug::error` fallback
