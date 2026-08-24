---
id: "003"
title: "Phase 2: inbound cutover"
status: done
created_at: 2026-08-23T08:31:12
last_modified: 2026-08-23T10:33:30
---

# 003: Phase 2: inbound cutover

## Description
static_on_rpc routes to core::Bridge: kEvent -> emitToLua, kCall -> stash RpcId + queue CommandCallMessage (async protocol via __coconut_rpc_receive). Replace dispatch::lifecycleEvent call sites with dispatcher->queue(LifecycleMessage). CFRunLoopSource perform -> flush() only; remove raw webview_eval path. Inject sync executor (lua::call) into Bridge for mt routing.
