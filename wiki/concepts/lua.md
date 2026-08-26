---
layout: default
title: "Lua"
---

# lua

## What it is
**LuaJIT 2.x** — the application authoring language for Coconut Milk. All app
logic (commands, view lifecycle, event handlers, config) is written in Lua and
executed in an embedded `lua_State` per thread.

## Why we use it
- Lightweight (~200 KB runtime) compared to Node.js / V8
- Fast JIT compilation — CPU-bound command logic stays performant
- Simple C API makes embedding and usertype registration straightforward
- Hot-reload friendly: `package.loaded[name] = nil` + re-`require`

## Key concepts
- **`coconut.config(ctx)`** — startup hook; receives the `CoconutContext`
  usertype and can mutate config via chainable setters
- **`coconut.views()`** — returns named view descriptors (`{ kind, src }`)
- **`ctx:bind(name, fn)`** — registers a command handler (runs on bg thread)
- **`ctx:bind_mt(name, fn)`** — registers a main-thread command (for platform
  APIs: dialog, clipboard, notify)
- **`ctx:emit({ name = "event", … })`** — fires an event through the three-tier
  Lua→JS dispatch chain
- **Generated `.g.lua` files** — produced by `coconut generate` from
  `---@command` annotations; auto-register command handlers via `register(ctx)`

## How we use it here
- `lua::Runtime` creates the main-thread state, opens standard libraries, and
  installs the `coconut` global table
- `core::WorkerPool` creates one background `sol::state` per worker
  (`apps/coconut/src/main.cpp`, trio wiring)
- `.g.lua` files are loaded into each worker state at startup by the pool's
  command initializer (`loadWorkerCommands`)
- The `ctx` global is set to `CoconutContext*` so generated files can call
  `ctx:bind`

## Gotchas
- **No shared state between threads**: the bg Lua state has its own
  `package.loaded`, its own globals, and its own command registry — a command
  defined on main is NOT visible on bg (and vice versa)
- **`coconut.config(ctx)` return value**: if it returns a table, additional
  scalar fields (`window_width`, `initial_view`, etc.) and a `views` block are
  merged into the shared `Config`
- **`main.lua` is optional**: if the file doesn't exist, the app runs on
  config-file defaults alone (not an error)
