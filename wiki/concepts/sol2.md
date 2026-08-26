---
layout: default
title: "Sol2"
---

# sol2

## What it is
[sol2](https://sol2.readthedocs.io) — a C++17/20 header-only binding library
between C++ and Lua (via LuaJIT in this project).

## Why we use it
- Type-safe C++ ↔ Lua interop without manual stack manipulation
- Usertype registration (`lua.new_usertype<T>`) maps C++ structs directly to
  Lua tables with methods
- `sol::protected_function` safely invokes Lua handlers and catches errors
- `sol::state_view` allows temporary Lua state access without ownership

## Key concepts
- **`sol::state`** — owns a `lua_State*`; created once per thread (main + bg)
- **`sol::state_view`** — non-owning reference, used for temporary access
- **`sol::protected_function`** — callable wrapper that returns
  `sol::protected_function_result` (check `.valid()` for errors)
- **Usertype** — `lua.new_usertype<T>("Name", "method", &T::method, …)` exposes
  C++ structs as Lua objects

## How we use it here
- `lua::Runtime` owns the main-thread `sol::state`
- Each `core::Worker` owns its own background `sol::state`
- `CoconutContext` is registered as a usertype via
  `context::registerUsertype` (single source of truth, used by the main
  runtime and every worker) with methods (`setWindowSize`, `bind`,
  `bind_mt`, `emit`, `show`, `setPosition`, …)
- `CoconutWindowHandle` is a separate usertype including live mutations
  (`setTitle`, `setMinimumSize`, `getPosition`, …)
- Command handlers are stored as `sol::protected_function` in
  `commands::Registry::handlers`
- JSON ↔ Lua table conversion uses `sol::state_view` + `sol::table`

## Gotchas
- **Thread safety**: a `sol::state` must only be accessed from its owning thread.
  The background thread has its own independent state — cross-thread Lua calls
  are not possible without serialization
- **`sol::lua_nil` vs `sol::nil`**: `sol::type::nil` is a type discriminator;
  `sol::lua_nil` is the actual Lua nil value
- **`#ifdef nil` undef**: some macOS system headers (ObjC transitive) define `nil`
  as a macro which clashes with sol2 — we `#undef nil` before including sol2
  headers
