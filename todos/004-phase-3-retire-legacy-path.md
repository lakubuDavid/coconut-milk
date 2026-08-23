---
id: "004"
title: "Phase 3: retire legacy path"
status: pending
created_at: 2026-08-23T08:31:12
last_modified: 2026-08-23T08:31:12
---

# 004: Phase 3: retire legacy path

## Description
Delete bg_thread (.g.lua loader moves into withCommands), dispatch::Outbox, dead bridge::dispatchRpcCallToLua. Repoint modules/bridge_emit at core::Bridge via runtime->bridge field. Shutdown ordering: pool shutdown -> dispatcher/bridge -> lua::destroy -> transport last. Verify mt_handlers vs handlers registry consistency.
