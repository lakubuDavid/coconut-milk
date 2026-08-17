---
layout: default
title: Bundle Pipeline
parent: Specifications
nav_order: 2
description: Package the app into a standalone distributable bundle.
---

# Bundle Pipeline — `coconut bundle`

Packages the app into a standalone distributable bundle.

---

## 1. Usage

```
coconut bundle [options]
```

### Options

| Flag | Long | Description |
|---|---|---|
| `--out-dir DIR` | — | Output directory (default: `build/bundle`) |
| `--bytecode-config` | — | Compile stripped config to .luac (B2 opt-in) |
| `-h` | `--help` | Show help |

### Example

```bash
coconut bundle --out-dir dist/MyApp.app
```

---

## 2. Pipeline overview

The bundle pipeline has four sequential steps:

```
1. writeShippableConfig()   — Strip dev fields, write config.json
2. icon_gen::generateIcons() — Auto-generate platform icons
3. generateManifests()      — Write Info.plist, app.manifest, .desktop
4. assembleBundle()         — Copy binary + config + assets into .app
```

If any step fails, the pipeline aborts and reports the error.

---

## 3. Step 1 — Config stripping

Reads the full dev config, strips dev-only fields, and writes a portability JSON
config (`coconut.config.json`) to the output directory.

### Stripped fields

| Field | Removed? | Notes |
|---|---|---|
| `debug` | yes | Whole block |
| `manifests` | yes | Whole block |
| `darwin.manifests` | yes | Reset to defaults |
| `win.manifests` | yes | Reset to defaults |
| `linux.manifests` | yes | Reset to defaults |
| `window_*`, `views`, `app.*`, `icon.*`, `darwin.ns.*` | no | Preserved |

### Output

The stripped config is written as JSON (not Lua) so the runtime can load it
without a Lua VM at startup.

Written to: `<out_dir>/coconut.config.json` (later relocated to
`<out_dir>/Contents/Resources/coconut.config.json` by Step 4).

---

## 4. Step 2 — Icon generation

If the user did not provide explicit `icon.*_path` fields, the pipeline
auto-generates platform icons from the configured `icon.source` file.

Supported source formats: SVG, PNG, JPEG, ICNS, ICO.

Generated files are written to `<out_dir>/`:

| Platform | Format | Filename |
|---|---|---|
| macOS | `.icns` | `icon.icns` |
| Windows | `.ico` | `icon.ico` |
| Linux | `.png` | `icon.png` |

If icon generation fails (e.g. missing source file, unsupported format), the
pipeline prints a warning but **does not abort** — the bundle continues without
icons.

See [Icon Generation](./icon-gen-spec.md) for the full icon generation spec.

---

## 5. Step 3 — Manifest generation

Generates platform-specific manifest files from the **original** (un-stripped)
config, because the `manifests.*` fields contain the manifest data.

### macOS

| File | Path |
|---|---|
| `Info.plist` | `<out_dir>/Contents/Info.plist` |
| `entitlements.plist` | `<out_dir>/Contents/entitlements.plist` (optional) |

The `Info.plist` contains:
- `CFBundleIdentifier`, `CFBundleName`, `CFBundleDisplayName`, `CFBundleVersion`,
  `CFBundleShortVersionString`, `CFBundlePackageType`, `CFBundleExecutable`
- `CFBundleIconFile` — always `icon`
- `NSHighResolutionCapable` — always `true`
- `LSMinimumSystemVersion` — `10.13`
- `LSApplicationCategoryType` — mapped from `app.category`
- `NSUserNotificationAlertStyle` — from `darwin.ns.notification_alert_style`
- `NS*UsageDescription` entries — from `darwin.ns.usage_descriptions`
- Extra keys — from `manifests.darwin_info_plist_extra`

### Windows

| File | Path |
|---|---|
| `app.manifest` | `<out_dir>/app.manifest` |

Contains assembly identity + common-controls dependency + supported OS GUIDs.

### Linux

| File | Path |
|---|---|
| `.desktop` file | `<out_dir>/<app.id>.desktop` |
| AppStream metainfo | `<out_dir>/<app.id>.metainfo.xml` |

---

## 6. Step 4 — Assembly

Creates the macOS `.app` bundle structure and copies files.

### Directory structure

```
<out_dir>/
└── Contents/
    ├── Info.plist                (from Step 3)
    ├── entitlements.plist        (from Step 3, if generated)
    ├── MacOS/
    │   └── coconut               (binary copy of running executable)
    └── Resources/
        ├── coconut.config.json   (from Step 1)
        ├── icon.icns             (from Step 2 or explicit path)
        ├── main.lua              (if exists in project root)
        ├── views/                (if view_root exists)
        ├── assets/               (if asset_root exists)
        └── commands/             (if command_root exists)
```

### Binary copying

- Finds the running executable path via `_NSGetExecutablePath` (macOS),
  `/proc/self/exe` (Linux), or `GetModuleFileNameA` (Windows)
- Sets executable permissions on the copied binary

### Resource copying

- Directories are copied recursively from the project root
- Missing source directories are silently skipped (may not exist per-project)

### Notes

- The stripped config is **moved** (not copied) from `<out_dir>/` into `Resources/`
- Icon is copied from the explicit path if set, or from the auto-generated location
- The `.app` bundle structure is macOS-specific. Windows/Linux bundle structure
  is planned for a future version.

---

## 7. C++ API

```cpp
namespace coconut::bundle {

std::expected<std::string, Error> bundle(
    const Config& cfg,
    const std::string& out_dir,
    bool bytecode_config = false);

std::expected<std::string, Error> writeShippableConfig(
    const Config& cfg, const std::string& out_dir);

std::expected<std::string, Error> generateManifests(
    const Config& cfg, const std::string& out_dir);

std::expected<std::string, Error> assembleBundle(
    const Config& cfg, const std::string& out_dir);

} // namespace coconut::bundle
```

All functions return a human-readable success message on success, or `Error`
with machine-readable `ErrorCode` on failure.
