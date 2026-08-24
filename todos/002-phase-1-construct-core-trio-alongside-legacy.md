---
id: "002"
title: "Phase 1: construct core trio alongside legacy"
status: done
created_at: 2026-08-23T08:31:12
last_modified: 2026-08-23T09:46:27
---

# 002: Phase 1: construct core trio alongside legacy

## Description
main.cpp after lua::create + transport: bg CoconutContext; WorkerPool::builder(n).withModules(bg).withCommands(.g.lua loader).withInitializer(set w->Context).build(); attachAll(); DispatcherBuilder with runtime/pool/transport; Bridge::builder with transport+lua state; setCommandCallHandler forwards to dispatcher->queue; store dispatcher+bridge on App. Legacy still runs everything.
