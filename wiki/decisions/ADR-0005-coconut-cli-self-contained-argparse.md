---
layout: default
title: ADR-0005 — Self-contained coconut-cli with p-ranav/argparse
parent: Architecture Decision Records
nav_order: 5
description: Copy shared generator/scaffolder sources into apps/coconut-cli and use p-ranav/argparse via xmake instead of include-path reuse of the coconut app tree.
---

# ADR-0005: Self-contained coconut-cli with p-ranav/argparse

**Status:** Accepted

**Date:** 2026-06-26

**Authors:** @david with assistant

## Context

`apps/coconut-cli` is a slim CI/editor front-end exposing only the command
generator and project scaffolder. It originally built by reaching into the
coconut app tree via include paths:

```lua
add_includedirs("../coconut/src")
add_files("../coconut/src/generators/main.cpp")
add_files("../coconut/src/new_project.cpp")
```

This made `coconut-cli` build-coupled to `apps/coconut`: any include
restructuring or header rename in the app tree broke the CLI build, and the
CLI could not be built or shipped independently. Its argv handling was also a
hand-rolled per-command loop in `src/main.cpp`.

A comparison of three header-only parsing libraries (Taywee/args,
morrisfranken/argparse, p-ranav/argparse) showed p-ranav/argparse is the only
one with native subcommand support, struct-friendly value binding
(`store_into` / typed `get`), and automatic help generation — the closest fit
to the table-driven parser already used inside the full `coconut` app.

## Decision

1. **Make `apps/coconut-cli` fully self-contained.** The dependency closure of
   the generator/scaffolder (`generators/main.cpp`, `generators/generate.h`,
   `generators/command_definition.hpp`, `generators/type_parser.hpp`,
   `new_project.{h,cpp}`, `utils.hpp`, `print.h`) is copied into
   `apps/coconut-cli/src/`. `xmake.lua` no longer references `../coconut/` at
   all. The copies are intentionally *forked*, not shared: the CLI must build
   and evolve independently of the app.

2. **Use p-ranav/argparse v3.2 from the xmake package manager** for argument
   parsing (`add_requires("argparse v3.2")`), not vendored under
   `thirdparty/`. The app's own hand-rolled parser stays in
   `apps/coconut/src/argparse.{h,cpp}` — it is a separate decision whether to
   migrate that too.

## Consequences

### Positive
- **Independent builds** — `apps/coconut-cli` builds with no knowledge of the
  app tree; renames/moves in `apps/coconut` no longer break the CLI.
- **Auto-generated help** — `coconut-cli generate --help` etc. come for free;
  the hand-rolled usage string duplication is gone.
- **Real option validation** — typed `get<std::string>` instead of manual
  string scanning.
- **Zero-maintenance dependency** — header-only, MIT, already in xmake-repo
  (installed with `xmake -y` on first configure).

### Negative
- **Source duplication** — generator/scaffolder code now exists in two trees.
  Changes to `apps/coconut/src/generators/*` must be manually mirrored into
  `apps/coconut-cli/src/generators/*`. Accepted because the CLI deliberately
  excludes the Lua runtime and is expected to diverge; revisit if the
  divergence burden becomes real.
- **libc++ C++26 caveat** — `std::format`-ing an `ArgumentParser` fails to
  compile under libc++ (deleted `formatter` constructor); help output must be
  streamed via `std::cerr << program` instead. Noted for future consumers of
  the library in this repo.

### Neutral
- `xmake f` now resolves one extra package on first build; no build-time cost
  (header-only).

## Alternatives Considered

- **Keep include-path reuse** (`../coconut/src`). Rejected: cross-tree
  coupling broke the "CLI builds standalone" property for the sake of avoiding
  ~1,200 lines of copied source.
- **Vendored `thirdparty/argparse/`** instead of xmake package. Rejected: the
  project already standardizes on `add_requires` for header-only deps
  (nlohmann_json, sol2); vendoring adds update burden for no offline-build
  need.
- **Taywee/args** — no subcommand support; API frozen. **morrisfranken/argparse** —
  ergonomic struct binding but weaker subcommand support and smaller project.
  Both rejected in favor of p-ranav/argparse.
