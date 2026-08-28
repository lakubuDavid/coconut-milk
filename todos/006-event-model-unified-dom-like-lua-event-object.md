---
id: "006"
title: "Event model: unified DOM-like Lua event object"
status: done
created_at: 2026-08-28T15:09:28
last_modified: 2026-08-28T15:09:28
---

# 006: Event model: unified DOM-like Lua event object

## Description
Lua table event with name/target/payload + preventDefault/stopPropagation; emit()/dispatch() in context.cpp + main_runtime.cpp.
