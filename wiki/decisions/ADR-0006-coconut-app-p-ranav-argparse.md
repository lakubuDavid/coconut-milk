---
layout: default
title: ADR-0006 — Migrate coconut app to p-ranav/argparse
parent: Architecture Decision Records
nav_order: 6
description: Replace the hand-rolled table-driven CLI parser in apps/coconut with p-ranav/argparse v3.2, keeping the Args struct as the contract.
---

# ADR-0006: Migrate coconut app to p-ranav/argparse

**Status:** Accepted

**Date:** 2026-06-26

**Authors:** @david with assistant

## Context

`apps/coconut` parses its command line with a hand-rolled, table-driven parser
(`src/argparse.h` + `src/argparse.cpp`, ~420 lines). It mirrors Python
`argparse` loosely: a static `Option OPTIONS[]` table with setter function
pointers, a `Subcommand SUBCOMMANDS[]` table (`generate`, `bundle`, `new`,
`run`), a single-pass parse loop, and five hand-written help printers
(`printHelp`, `printGenerateHelp`, `printBundleHelp`, `printNewHelp`,
`printRunHelp`).

The companion `apps/coconut-cli` was already migrated to p-ranav/argparse v3.2
via xmake (ADR-0005). Analysis of the three candidate libraries
(Taywee/args, morrisfranken/argparse, p-ranav/argparse) showed p-ranav is the
only one with native subcommand support, typed value binding, real option
validation, and automatic help generation — and is therefore the closest fit
for this parser's semantics.

Key findings from auditing current usage of `coconut::argparse::Args`:

- Every consumer of the parser lives in `src/main.cpp` alone (only it includes
  `argparse.h` and calls `parse()`). The `coconut` and `tests` xmake targets
  both compile `src/*.cpp` (so `argparse.cpp`), while `test_modules_*` targets
  list files explicitly and do not.
- `Args` is the contract: ~40 field reads in `main.cpp` (subcommand booleans,
  `root`, `out_dir`, `watch`, `new_name`, `template_name`, `yes`, the
  `override_*` window fields, `debug`, `bytecode_config`) plus the run-mode
  pass-through (`positional_args` → the `coconut.args.positional` Lua/JS table).
- `Args::key_value_args` and `Args::flag_args` are **dead**: declared and read
  into Lua/JS tables in `main.cpp`, but the parser never populates them.
  Confirmed by repo-wide grep: no consumer outside `main.cpp` references
  `coconut.args.named` / `coconut.args.flags`.
- The current `--` separator `break`s and **discards everything after it** — a
  latent bug for run-mode app-argument pass-through.

## Decision

Migrate `apps/coconut` to **p-ranav/argparse v3.2** (header-only, via the xmake
package manager, mirroring the CLI), using the **lowest-churn approach**:

1. **Keep the `Args` struct as the contract.** Reimplement
   `coconut::argparse::parse(int, char**)` on top of p-ranav and have it fill
   the same `Args` struct. `main.cpp` is not rewritten to call p-ranav
   directly — only its help-dispatch block and the dead `named`/`flags`
   injections are removed. This avoids touching ~40 field accesses and the
   Lua/JS arg-injection block.

2. **Use p-ranav subparsers** (`add_subparser`) for `generate`, `bundle`,
   `new`, `run`, replacing `SUBCOMMANDS[]`. Per-subcommand `--help` is then
   automatic — the five hand-written help printers are deleted.

3. **Add global options** to the root parser: `-h/--help`, `-v/--version`,
   `-d/--debug`, `-r/--root <dir>`, `-o/--out-dir <dir>`, `--frameless`,
   `--transparent`, `--bytecode`, `-y/--yes`, `--watch`, `--title <t>`,
   `--window-width <int>`, `--window-height <int>`. Integer overrides use
   `.scan<'i', int>()` so invalid input throws (today it uses `std::atoi`,
   which silently yields 0 and masks user error).

4. **Run-mode pass-through via `parse_known_args()`.** Unrecognized trailing
   args are collected into `Args::positional_args`, which feeds
   `coconut.args.positional`. This fixes the latent `--` discard bug: args
   after `--` are now captured instead of dropped.

5. **Drop dead fields.** `Args::key_value_args` and `Args::flag_args` are
   removed from the struct and from `main.cpp`'s Lua/JS injection.

6. **Version string:** passed to the `ArgumentParser` constructor
   (`COCONUT_VERSION`), and `-v/--version` continues to feed `args.version`
   handled in `main.cpp` (output stays `"<prog> <VERSION>"`).

7. **`xmake.lua`:** add `add_requires("argparse v3.2")` and `add_packages(
   "argparse")` to the `coconut` and `tests` targets only.

## Consequences

### Positive
- **Deletes ~350 lines** of hand-rolled parser (tables, setters, parse loop,
  help printers) and replaces with declarative p-ranav calls.
- **Validation fix:** `--window-width abc` now errors instead of silently
  becoming 0.
- **`--` pass-through fix:** trailing app args after `--` are captured into
  `coconut.args.positional`.
- **Auto help** that is subcommand-aware and consistent with `coconut-cli`.
- **Single parser dependency** across both binaries (CLI + app), same version,
  same integration pattern.
- No `thirdparty/` vendoring; header-only via xmake-repo.

### Negative
- **Dead-field removal is a (tiny) breaking change** to `coconut.args` shape:
  `named`/`flags` keys disappear. Already confirmed unreferenced repo-wide, so
  impact is nil.
- **Help text changes** from the custom prose to p-ranav's generated layout.
- **libc++ C++26 caveat** (carried from ADR-0005): an `ArgumentParser` cannot
  be passed through `std::format`/`println("{}", program)` — help must be
  streamed with `std::cerr << program`. Enforced in the new `parse()`.

### Neutral
- `main.cpp` keeps the `Args` struct access pattern; the migration is contained
  to `argparse.{h,cpp}` plus a small `main.cpp` trim.

## Alternatives Considered

- **Approach B — full native p-ranav in `main.cpp`** (drop `Args`, query the
  parser directly). Rejected: ~40 field reads and the Lua/JS injection block
  already depend on the `Args` struct; rewriting them adds churn and review
  surface for no behavioral gain.
- **Keep the hand-rolled parser.** Rejected: it already duplicates help text
  across the option table and the printer functions, lacks validation, and
  carries the `--` discard bug. ADR-0005 already standardized the app family
  on p-ranav.
- **Taywee/args / morrisfranken/argparse.** Rejected for the same reasons as
  ADR-0005 — no native subcommands (Taywee) or thinner subcommand support and
  smaller project (morrisfranken).
