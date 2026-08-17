---
layout: default
title: ADR-0004 — Single shared bg→main outbox for forwarding
parent: Architecture Decision Records
nav_order: 4
description: Keep one shared bg→main outbox for both CommandResult and ForwardRequest; split later if needed.
---

# ADR-0004: Single shared bg→main outbox for forwarding

**Status:** Accepted

**Date:** 2026-06-25

**Authors:** Grill session between @david and assistant

## Context

The background thread communicates with the main thread through two lock-free SPSC outboxes:

- `bg->inbox` (Main→Bg): carries `CommandCall`, `BgCommandCall`, `ForwardResult`, `ForwardError`
- `bg->outbox` (Bg→Main): carries `CommandResult`, `ForwardRequest`, `EmitEvent`

With the addition of async cross-thread forwarding, `bg->outbox` now carries **two traffic classes**:

1. **CommandResult** — normal results from user commands back to JS (via `WebviewTransport`)
2. **ForwardRequest** — requests to execute a main-thread API (dialog, notify, etc.)

Both share the same ring buffer (capacity 64). This raises the question: should they share one queue or have dedicated queues?

## Decision

**Keep a single shared `bg->outbox` for v1.** Both traffic classes share the capacity-64 ring buffer. No change to the dispatch infrastructure.

If monitoring later shows one traffic class starving the other (e.g., a burst of dialog requests blocking command results from reaching JS), the outbox can be split into two dedicated queues — one per message kind — without affecting any other code.

## Consequences

### Positive
- **Simpler code** — one outbox, one drain loop, no multiplexing
- **No changes to lock-free queue** — the existing SPSC `Outbox` class is unchanged
- **Sufficient capacity** — 64 slots is generous for both traffic classes combined. A single forwarding chain (Request + Result) uses 2 slots. Even with 10 concurrent forwarded calls and 10 command results, only 20 of 64 slots are used.
- **Quick to split later** — splitting into two outboxes is a 10-minute mechanical change that touches only `bg_thread.h` (add one field) and `dispatch.cpp` (add one `while` loop).

### Negative
- **No isolation** — a burst of forwarding requests (e.g., rapid `dialog.open` calls) could fill the queue and block command results. In practice, dialog calls block on user interaction (seconds), so they can't burst.
- **Harder to monitor** — can't distinguish which traffic class is filling the queue without inspecting message kinds.

### Neutral
- If queue pressure is ever observed, the split can happen at any time without a migration.

## Alternatives Considered

- **Two dedicated outboxes** (`bg->result_outbox` + `bg->forward_outbox`). Rejected for v1 because the added complexity of managing two queues is not justified without evidence of starvation. The ADR document is the record that this option exists for later.

- **Dynamic capacity** — grow the ring buffer when full. Rejected because the lock-free SPSC design requires fixed capacity at compile time. Dynamic growth would require a mutex.
