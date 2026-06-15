# ADR-0001: Quit via synchronous webview_terminate in Lua handler

**Status:** Accepted

**Date:** 2026-06-16

**Authors:** Coconut Milk team

## Context

The app crashed with `EXC_BAD_ACCESS` on quit (`mod+q` / `alt+f4`). The crash
occurred in `engine_base::dispatch()` during destruction — specifically, a
`webview_return` callback (queued via `dispatch_async`) was processed after
`m_webview` was released.

Two failed approaches:
1. `__quit` bridge command that called `webview_terminate` then `webview_return`.
   The GCD callback from `webview_return → resolve → dispatch` survived into
   `deplete_run_loop_event_queue()` after `m_webview` was nil.
2. `app->quitting` flag checked at the top of `dispatch()`. The queued GCD
   callback still ran after the flag was set, but `m_webview` was already
   released.

## Decision

`webview_terminate` is called **synchronously inside the Lua `__coconutWindowCtl`
handler**, before `respond()` is called. No `webview_return` is ever queued during
quit. The Lua handler does not call `respond()` for the `close` action.

```
JS: coconut.keybind → coconut.window.close()
  → coconut.call("__coconutWindowCtl")
    → Lua ctx:bind handler
      → CoconutWindowHandle::close()
        → webview_terminate(app->webview)   ← synchronous, no respond()
          → webview_run loop returns
```

`coconut.window.close()` and `coconut.quit()` are both aliases for the same
synchronous path.

## Consequences

### Positive
- Clean shutdown, no `EXC_BAD_ACCESS`.
- No `quitting` flag needed in `app` struct.
- Simple mental model: `close()` is synchronous termination, no callbacks.

### Negative
- No opportunity for listeners to intercept or veto quit.
- Any cleanup that requires async work (e.g. saving files with async I/O)
  must be done before the keybind fires.
- Architecture does not yet support the `quit` event emission described in the
  original plan. This is a deferred feature.

### Neutral
- The `__quit` bridge command and `app->quitting` flag were removed.
- Platform keybind handlers (`keyboard.mm`) still dispatch to Lua and JS on
  quit, but since `webview_terminate` runs synchronously in Lua, the WebView
  is still alive when those dispatches fire.