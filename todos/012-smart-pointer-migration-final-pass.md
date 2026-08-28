---
id: "012"
title: "Smart-pointer migration final pass"
status: pending
created_at: 2026-08-28T15:09:28
last_modified: 2026-08-28T15:09:28
---

# 012: Smart-pointer migration final pass

## Description
Wrap raw 'new Store()' in store.cpp:7; confirm Store lifetime ownership (sol-managed vs leaked). Dispatcher/Bridge/WorkerPool already use unique_ptr/shared_ptr.
