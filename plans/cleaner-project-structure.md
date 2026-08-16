# Cleaner Project Structure — Plan

**Status:** Draft · **Targets:** v0.2.0 (mechanical), v0.3.0 (policy)
**Owner:** coconut-milk
**Related:** `mise.toml`, `Justfile`, `xmake.lua`, `.gitignore`

---

## 1. Goal

Reduce root-level clutter, fix naming collisions, and give `src/` a predictable
layout so the build config (`xmake.lua`) and the mental model match the folder
structure. **Non-goal:** moving code for its own sake — every move must be
mechanical and low-risk.

## 2. Current state (measured, not guessed)

| Path | What it is | Problem |
|---|---|---|
| `src/*.cpp/h` (~53 files) | Runtime: app, bridge, dispatch, context, config, argparse, main, window, webview_transport, hotreload, fs, routes, permissions, store, … | Flat mix — no grouping |
| `src/core/` | `worker.{cpp,h}` only | Good precedent, nearly empty |
| `src/modules/` | Lua-facing bindings (`coconut.dialog`, `fs`, …) | OK |
| `src/platform/` | per-OS adapters (darwin/win/linux) | OK |
| `src/packages/` | C++ wrappers (`coconut::packages`) | **Name collides with root `packages/`** |
| `src/generators/`, `src/embeds/` | codegen, TS/JS embeds | OK |
| `packages/` (root) | npm package `coconut-vite` | Collides with `src/packages/` |
| `examples/` | 6 reference apps — **778 tracked files, 685 of them `node_modules`** | `node_modules` committed |
| `samples/` (52 files) | sample project(s), overlapping `examples/` | Unclear boundary vs `examples/` |
| `test-x/` (12 files) | scratch manual-test project | Scratch code committed at root |
| `.cache/` | **37 tracked clangd `.idx` files** | Build cache committed |
| `objects/` | **`objects/generator` tracked (a build artifact, `file` = data)** | Build artifact committed |
| `bin/` | old binary output (gitignored) | Stale — xmake now builds into `build/` |
| `generated/` | pipeline glue (`.g.lua`, `.d.ts`, `.g.js`) | Committed by design (AGENTS.md) but at root |
| `compile_commands.json` | xmake plugin output, root, gitignored | OK; could move under `build/` |
| `tmp/` | scratch, gitignored | OK |

## 3. Target structure

```
coconut-milk/
├── src/
│   ├── core/            # runtime + bridge + dispatch + config + window
│   │   ├── app.*  bridge.*  dispatch.*  config.*  argparse.*
│   │   ├── window.*  webview_transport.*  commands.*  routes.*
│   │   └── main.cpp
│   ├── lua/             # Lua runtime + context + worker
│   │   ├── main_runtime.*  bg_runtime.*  context.*
│   │   ├── worker.*  view_events.*  store.*
│   │   └── …
│   ├── modules/         # Lua-facing bindings (coconut.* tables)
│   ├── platform/        # per-OS adapters (darwin/win/linux)
│   ├── packages/        # C++ helpers (coconut::packages)
│   ├── generators/      # codegen
│   └── embeds/          # TS/JS + generated embed headers
├── tests/               # C++ suite (+ tests/fixtures/ for manual test apps)
├── examples/            # reference apps (node_modules gitignored)
├── thirdparty/          # vendored deps (webview submodule)
├── scripts/             # dev + release tooling (mise-install.sh, …)
├── docs/                # docs + diagrams
├── plans/               # planning docs
├── schemas/  res/  patches/
├── build/               # xmake artifacts (gitignored)
├── generated/           # build-pipeline glue (committed; see §5 C)
├── xmake.lua  mise.toml  Justfile
```

## 4. Phase 0 — config-only hygiene (safe now, no code moves)

> Partially done in this session: `mise.toml` tasks + `scripts/mise-install.sh`
> + `Justfile` `TEST_TARGET` fix (`coconut-milk-tests` → `tests`).

1. **`.gitignore`** add:
   - `.cache/` (clangd index)
   - `objects/`
   - `examples/*/node_modules/` and `packages/*/node_modules/`
   - `.mise/`
2. **`git rm --cached`** the wrongly-tracked files:
   - `.cache/` (37 clangd `.idx` files)
   - `examples/**/node_modules` (685 files)
   - `objects/generator`
   - `test-x/` (move to `tests/fixtures/` first — see Phase 1)
3. Verify `mise run test` works with the corrected target name.

**Risk:** near-zero (no source changes). **Effort:** minutes.

## 5. Phase 1 — mechanical moves (v0.2.0)

1. **Group `src/` top-level files** into `src/core/` and `src/lua/` as shown in
   §3. Update `xmake.lua`: replace `add_files("src/*.cpp")` with
   `add_files("src/core/*.cpp", "src/lua/*.cpp")` (+ any stragglers).
   - Include paths are already `src/`-relative (`#include "app.h"`), so
     **no `#include` changes needed** — only the glob in `xmake.lua`.
2. **Rename root `packages/` → `npm/`** to kill the collision with
   `src/packages/`. Update any references (CI, docs, `create-coconut-app`).
3. **Move `test-x/` → `tests/fixtures/manual/`** (it's a scaffolded manual-test
   app) or delete it if superseded by `tests/`.
4. **Consolidate `samples/` into `examples/`** (or `tests/fixtures/` if they are
   test fixtures). Delete the now-empty directory.

**Risk:** low (mechanical; one CI run to confirm). **Effort:** half a day.

## 6. Phase 2 — policy changes (v0.3.0+, optional)

1. **`generated/`**: keep committed (AGENTS.md: generated files are part of the
   build pipeline) but consider moving under `build/generated/` + CI regeneration
   so the repo never holds stale glue. Decide before v0.3.
2. **`bin/`**: delete — xmake already outputs to `build/`; remove the stale
   `.gitignore` entry once confirmed nothing references it.
3. **`compile_commands.json`**: point xmake's plugin at `build/`
   (`outputdir = "build"`); document `clangd -p build` for contributors.

## 7. Deliberate keepers (do NOT move)

- `thirdparty/` (webview submodule + patch in `patches/`)
- `schemas/`, `res/`, `docs/`, `plans/`
- `.agents/`, `.pi/` (agent tooling), `.github/` (CI)
- Root `xmake.lua` (build config stays discoverable at root)

## 8. Open questions

1. `src/core/` vs `src/lua/` split — or keep a single `src/core/` for both
   (runtime + Lua)? The `worker` file currently lives in `core/`; if it's the
   bg-thread worker it may belong in `lua/`.
2. Should `generated/` move now (Phase 2) or stay at root for v0.2?
3. `samples/` — fixtures or reference apps? Determines where they land.

## 9. Success criteria

- `git ls-files | awk -F/ '{print $1}'` shows a clean top level (no `.cache`,
  `objects`, `test-x`, `node_modules`).
- `xmake.lua` file globs match the real folder layout (no dead `src/core/*.cpp`
  when `src/core` didn't exist).
- `mise run build` / `mise run test` / `just build` all work from a fresh clone
  after a full `git rm --cached` + re-add cycle.

## 10. Execution (v0.1.1 — supersedes §5/§6 as executed)

Executed in the `apps/coconut` monorepo move. Differences from the plan above:

- **C++ app relocated to `apps/coconut/`** (src, tests, res, xmake.lua, Justfile),
  not grouped into `src/core` + `src/lua`. `src/core` was **left as-is**
  (contains only the WIP `worker.{cpp,h}`, untouched, uncommitted).
- **Mise monorepo**: root `mise.toml` sets `monorepo_root = true`; app tasks
  live in `apps/coconut/mise.toml` (namespaced `//apps/coconut:<task>`).
- **`test-x/`** moved to `apps/coconut/tests/fixtures/manual/`.
- **`samples/`** and **`generated/`** left at the repo root as shared content
  (follow-up: fold `samples/` into `examples/`, decide `generated/` policy).
- **`packages/` → `npm/` rename deferred** (still root `packages/`).
- Phase 0 hygiene done: `.cache/`, `objects/`, `examples/*/node_modules`
  untracked; `.gitignore` extended.

**Known gap:** `xmake.lua` still globs `src/core/*.cpp`, but `src/core` is
uncommitted — a fresh clone will not build until `worker.*` is committed or the
glob is removed.
