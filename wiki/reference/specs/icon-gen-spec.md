---
layout: default
title: Icon Generation
parent: Specifications
nav_order: 5
description: Auto-generate platform-specific icon files from a single source image.
---

# Icon Generation — `icon_gen`

Auto-generates platform-specific icon files from a single source image.

---

## 1. Purpose

`coconut bundle` needs platform-specific icon formats:
- **macOS**: `.icns` (IconServices format)
- **Windows**: `.ico` (icon resource format)
- **Linux**: `.png` (freedesktop-compatible)

Rather than requiring the developer to provide all three, the framework can
generate them from a single source file.

---

## 2. Source format support

| Format | Supported | Notes |
|---|---|---|
| SVG | yes | Converted via lunasvg rasterizer |
| PNG | yes | Wrapped directly into target formats |
| JPEG | yes | Decoded via stb_image, converted to PNG internally |
| ICNS | yes | Passed through directly for macOS; other platforms extract embedded PNG |
| ICO | yes | Passed through directly for Windows; other platforms extract embedded PNG |

---

## 3. Generated files

Given `icon.source = "assets/app-icon.svg"` and `app.id = "com.example.myapp"`:

| Platform | File | Location |
|---|---|---|
| macOS | `icon.icns` | `<out_dir>/icon.icns` |
| Windows | `icon.ico` | `<out_dir>/icon.ico` |
| Linux | `icon.png` | `<out_dir>/icon.png` |

Generated filenames are fixed (`icon.*`), not derived from `app.id`. The icon
files are later copied into the bundle by `assembleBundle()`.

---

## 4. Quality / sizing

### SVG source

- Rendered at 1024×1024 using lunasvg
- Downscaled to required sizes for each target format

### PNG source

- Used as-is for Linux
- For ICNS: embedded in icon family at native resolution
- For ICO: embedded if size ≤ 256×256 (Windows icon limit per frame)

### ICNS / ICO source

- Passed through directly for their native platform
- For other platforms: embedded PNG is extracted (first frame for ICO)

---

## 5. Fallback

If `icon.source` is empty and no explicit `*_path` fields are set, `coconut bundle`
uses the **embedded default icon**. This is a compiled-in SVG + pre-rendered PNG
stored in `src/embeds/default_icon_svg.h` and `src/embeds/default_icon_png.h`.

The default icon is a simple Coconut Milk logo (a coconut with a drop of milk).

---

## 6. Skipping generation

Icon generation is skipped if the user provides explicit paths for all three
platforms (`icns_path`, `ico_path`, `png_path`). In that case, those paths
are used directly during bundle assembly.

---

## 7. Error handling

Icon generation failures are **non-fatal**. The bundle pipeline prints a warning
and continues without icons. This allows the developer to manually place icon
files later.

```cpp
auto icons = icon_gen::generateIcons(cfg.icon.source, out_dir, app_id);
if (!icons) {
  std::println(stderr, "bundle: warning: icon generation failed: {}",
               icons.error().message);
}
```

---

## 8. C++ API

```cpp
namespace coconut::icon_gen {

struct IconResult {
  std::string icns;  ///< Path to generated .icns, or empty
  std::string ico;   ///< Path to generated .ico, or empty
  std::string png;   ///< Path to generated .png, or empty
};

/// Auto-generate platform icons.
///
/// @param source   Path to source image file (SVG, PNG, JPEG, ICNS, ICO).
///                 Empty = use embedded default.
/// @param out_dir  Output directory for generated files.
/// @param app_id   App identifier (used for fallback naming only).
/// @return         IconResult with paths to generated files, or Error.
std::expected<IconResult, Error> generateIcons(
    const std::string& source,
    const std::string& out_dir,
    const std::string& app_id);

} // namespace coconut::icon_gen
```
