---
id: "001"
title: "Phase 0: prerequisites for wiring"
status: done
created_at: 2026-08-23T08:31:12
last_modified: 2026-08-23T09:27:10
---

# 001: Phase 0: prerequisites for wiring

## Description
Add RpcId to core::CommandCallMessage (promise correlation); WebviewTransport creatable as shared_ptr (factory), legacy bridge::State holds shared_ptr; static_on_rpc respects setMessageCallback when set (fallback to direct handleCall/handleEvent). Build green + unit tests pass.
