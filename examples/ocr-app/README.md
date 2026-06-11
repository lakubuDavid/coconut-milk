# OCR App Example

A Coconut Milk example app that uses Tesseract OCR to scan documents, receipts, and images. The frontend uses **Alpine.js** for reactivity and **UnoCSS** for styling.

## Prerequisites

- [Tesseract](https://tesseract-ocr.github.io) — `brew install tesseract` on macOS
- Bun (for the frontend CDN assets, no build step needed)

## Run

```bash
# From the Coconut Milk project root:
just run-ocr
# or:
xmake build ocr-app
xmake run ocr-app
```

## Features

- **Drag & drop** or click to upload an image
- **Image preview** before scanning
- **OCR** via Tesseract CLI called from Lua
- **Copy & download** extracted text
- **Window size** tracking — Lua emits `window_resized` events, frontend displays current size
- **Minimum window size** enforced (600×500)
- **Standard framed window** (not frameless)

## Commands

| Command | Description |
|---|---|
| `ocr_save_temp` | Save a base64-encoded image to a temp file |
| `ocr_scan` | Run Tesseract OCR on an image path |
| `ocr_save_text` | Save extracted text to a file |

## Architecture

```
Frontend (Alpine.js + UnoCSS)
  ↓  file drop / base64
  ↓  coconut.call("ocr_save_temp", ...)
  ↓  coconut.call("ocr_scan", ...)
  ↓  coconut.call("ocr_save_text", ...)
Lua Bridge (sol2)
  ↓  io.popen("tesseract ...")
Tesseract CLI
  ↓  stdout / file
Extracted Text
```

## View lifecycle

The `scan` view has `on_load`, `on_mount`, and `on_unmount` callbacks that log to the console.
