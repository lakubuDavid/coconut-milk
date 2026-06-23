---
layout: default
title: Bundle Your App
parent: Guides
nav_order: 1
description: Package your Coconut Milk app into a standalone distributable bundle.
---

# How to: Bundle Your App for Distribution

This guide shows how to package your Coconut Milk app into a standalone distributable bundle using the `coconut bundle` command.

---

## What you'll learn

- Run the bundle pipeline
- Configure app metadata for distribution
- Understand the bundle output structure
- Platform-specific bundle formats (macOS .app)
- Icon generation and manifest files

---

## Quick start

Once your app is ready for distribution, run:

```bash
coconut bundle --out-dir dist/MyApp.app
```

This produces a standalone macOS `.app` bundle at `dist/MyApp.app` containing your binary, config, assets, and views.

---

## Prerequisites

Before bundling, make sure your `coconut.config.lua` has the required app metadata:

```lua
function coconut.config(ctx)
  return ctx
    :setTitle("My App")
    :setWindowSize({ w = 1024, h = 768 })
end

-- App metadata (used for bundle manifests)
coconut.app = {
  id          = "com.example.myapp",
  name        = "My App",
  version     = "1.0.0",
  author      = "Your Name",
  description = "What my app does",
  category    = "public.app-category.productivity",
}
```

Required fields:
- `app.id` — Reverse-domain identifier (e.g., `com.example.myapp`)
- `app.name` — Display name
- `app.version` — Version string
- `app.author` — Author name
- `app.description` — Short description

---

## Bundle pipeline

The `coconut bundle` command runs four steps:

```
1. Write stripped config    → Removes dev-only fields, outputs JSON
2. Generate icons           → Auto-generates platform icons from source
3. Generate manifests       → Writes Info.plist, app.manifest, .desktop
4. Assemble bundle          → Copies binary + config + resources
```

### Step 1: Config stripping

Dev-only fields like `debug`, `manifests` blocks are stripped. The remaining config is written as JSON so the runtime can load it without a Lua VM.

### Step 2: Icon generation

If you haven't set explicit icon paths, the pipeline auto-generates platform icons from your `icon.source` file. Supported source formats: SVG, PNG, JPEG, ICNS, ICO.

Configure icons in your app metadata:

```lua
coconut.icon = {
  source = "assets/icon.svg",     -- Source file
  -- Or provide explicit paths:
  -- icns_path = "assets/icon.icns",
  -- ico_path = "assets/icon.ico",
  -- png_path = "assets/icon.png",
}
```

If icon generation fails, the pipeline prints a warning but **does not abort**.

### Step 3: Manifest generation

Platform-specific manifest files are generated:

| Platform | Files |
|---|---|
| macOS | `Contents/Info.plist`, `Contents/entitlements.plist` (optional) |
| Windows | `app.manifest` |
| Linux | `.desktop` file, AppStream metainfo |

The `Info.plist` includes bundle identifier, name, version, icon reference, and any usage description strings you've configured.

### Step 4: Assembly

The final bundle structure looks like this:

```
<out-dir>/
└── Contents/
    ├── Info.plist
    ├── MacOS/
    │   └── coconut          (the runtime binary)
    └── Resources/
        ├── coconut.config.json
        ├── icon.icns
        ├── main.lua          (if exists)
        ├── views/            (if view_root exists)
        ├── assets/           (if asset_root exists)
        └── commands/         (if command_root exists)
```

---

## Options

| Flag | Description |
|---|---|
| `--out-dir DIR` | Output directory (default: `build/bundle`) |
| `--bytecode-config` | Compile stripped config to Lua bytecode (opt-in) |
| `-h, --help` | Show help |

Example with custom output:

```bash
coconut bundle --out-dir dist/MyApp-1.0.0.app
```

---

## Platform notes

### macOS

- The `.app` bundle structure is macOS-specific
- The binary is copied using `_NSGetExecutablePath` and permissions are set
- Entitlements are generated from config if present

### Windows & Linux

- Bundle structure for Windows/Linux is planned for a future version
- Currently, you can manually copy the binary and resources

---

## Testing your bundle

After bundling, test the standalone app:

```bash
open dist/MyApp.app              # macOS
./dist/MyApp.app/Contents/MacOS/coconut  # Direct binary launch
```

---

## Next steps

- Read the [Bundle Specification](../reference/specs/bundle-spec.md) for complete pipeline details
- See [Icon Generation](../reference/specs/icon-gen-spec.md) for icon format requirements
- Check [Config Specification](../reference/specs/config-spec.md) for all config fields
