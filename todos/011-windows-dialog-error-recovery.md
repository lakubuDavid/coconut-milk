---
id: "011"
title: "Windows dialog error recovery"
status: pending
created_at: 2026-08-28T15:09:28
last_modified: 2026-08-28T15:09:28
---

# 011: Windows dialog error recovery

## Description
Add try/catch (or HRESULT checks) around Win32 native dialog calls in platform/win/dialog.cpp. Scope needs GTK/Win32 coverage; Linux+macOS done, Windows has zero error handling.
