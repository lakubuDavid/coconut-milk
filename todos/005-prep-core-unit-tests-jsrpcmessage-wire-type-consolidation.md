---
id: "005"
title: "Prep: core unit tests + JsRPCMessage wire-type consolidation"
status: done
created_at: 2026-08-23T09:14:23
last_modified: 2026-08-23T09:14:23
---

# 005: Prep: core unit tests + JsRPCMessage wire-type consolidation

## Description
FakeTransport-based unit tests for core::Bridge (builder validation, emitToJS/rpcSend envelopes, forwardCommandCall handler) and core::Dispatcher (builder validation, JsCall→transport, CommandCall→pool, Lifecycle no-op safety). Switched transport.h/WebviewTransport/legacy bridge+dispatch to core::JsRPCMessage. Fixed BridgeBuilder eager sol::state_view(nullptr) SEGV via std::optional; excluded module-test mains from aggregate tests target; migrated stale escapeJsSingleQuotedString tests.
