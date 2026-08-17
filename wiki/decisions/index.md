# Design Decisions

Architecture, design decisions, ADRs, and troubleshooting for Coconut Milk.

## Architecture Decision Records

| Number | Title |
|---|---|
| [ADR-0001](ADR-0001-quit-via-synchronous-webview-terminate.md) | Quit via synchronous webview terminate |
| [ADR-0003](ADR-0003-linux-platform.md) | Linux platform support |
| [ADR-0004](ADR-0004-single-bg-outbox-for-now.md) | Single bg→main outbox for now |

## Internal Concepts

- **[concepts.md](concepts.md)** — layered architecture, view system, bridge protocol overview, event model, platform support
- **[event-dispatch-model.md](event-dispatch-model.md)** — three-tier dispatch chain, event objects, propagation
- **[roadmap.md](roadmap.md)** — implementation phases and milestones
- **[troubleshooting.md](troubleshooting.md)** — common errors, solutions, debug tips

## ADR Template

See [TEMPLATE.md](TEMPLATE.md) for the standard ADR format.
