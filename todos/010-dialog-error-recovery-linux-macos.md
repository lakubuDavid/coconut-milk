---
id: "010"
title: "Dialog error recovery: Linux + macOS"
status: done
created_at: 2026-08-28T15:09:28
last_modified: 2026-08-28T15:09:28
---

# 010: Dialog error recovery: Linux + macOS

## Description
try/catch around GTK (linux/dialog.cpp) + @try/@catch around AppKit (darwin/dialog.mm). Win32 still missing.
