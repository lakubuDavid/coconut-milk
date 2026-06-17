# Config Schema v2

Extended runtime configuration beyond the v0 flat schema. These fields are parsed
from both `coconut.config.lua` and `coconut.config.json`.

---

## 1. App identity — `app`

Cross-platform app identity defaults. Each platform can override individual fields.

```lua
return {
  app = {
    name        = "My App",          -- Display name (menus, window titles)
    id          = "com.example.app", -- Bundle identifier / AppUserModelID / .desktop id
    version     = "1.0.0",           -- Semver
    description = "A Coconut app",   -- Short description (desktop entries)
    category    = "developer-tools", -- High-level category (maps to per-platform taxonomy)
  },
}
```

### Category mapping

| Coconut category | macOS `LSApplicationCategoryType` | Linux freedesktop category |
|---|---|---|
| `developer-tools` | `public.app-category.developer-tools` | `Development` |
| `utility` | `public.app-category.utilities` | `Utility` |
| `productivity` | `public.app-category.productivity` | `Office` |
| `education` | `public.app-category.education` | `Education` |
| *(others)* | — | passthrough as-is |

### Override semantics

- `darwin.app.*`, `win.app.*`, `linux.app.*` override the shared `app.*` fields for that platform
- Empty strings = use default (no override)

---

## 2. Icon — `icon`

Icon references per platform. The `bundle` command uses these to produce platform-specific icon files.

```lua
return {
  icon = {
    source    = "assets/icon.svg",  -- Single source file (SVG, PNG, JPEG, ICNS, ICO)
    icns_path = "build/icon.icns",  -- macOS explicit .icns (overrides auto-gen)
    ico_path  = "build/icon.ico",   -- Windows explicit .ico
    png_path  = "build/icon.png",   -- Linux explicit .png (or freedesktop icon name)
  },
}
```

### Resolution order

1. If explicit `*_path` is set → use it directly
2. If `source` is set → `coconut bundle` auto-generates the platform formats
3. If neither → `coconut bundle` falls back to the embedded default icon

See [Icon Generation](./icon-gen-spec.md) for the auto-generation pipeline.

---

## 3. macOS system permissions — `darwin.ns`

```lua
return {
  darwin = {
    ns = {
      notification_alert_style = "alert",  -- "alert" | "banner" | "none"
      usage_descriptions = {
        NSCameraUsageDescription        = "Need camera for scanning",
        NSMicrophoneUsageDescription    = "Need mic for recording",
        NSPhotoLibraryUsageDescription  = "Need photo access for import",
        -- ... any NS*UsageDescription key
      },
    },
  },
}
```

The `ns` block is only meaningful on `darwin`. It is accepted on `win` / `linux` but
silently ignored.

---

## 4. Bundling / packaging hints — `manifests`

Dev-time configuration for the `coconut bundle` pipeline. Stripped from the shipped config.

```lua
return {
  manifests = {
    strip_dev_fields = true,          -- Strip debug, generators, manifests.* from shipped config
    bytecode_config  = false,         -- Compile stripped config to .luac (B2 opt-in)
    target_archs     = {"x86_64", "arm64"},  -- Multi-arch bundle targets

    -- Extra keys merged into generated platform manifests
    darwin_info_plist_extra = {
      NSSupportsAutomaticTermination = "NO",
    },
    darwin_entitlements = {
      "com.apple.security.device.camera" = "(allow com.apple.coremedia.camera)",
    },

    linux_desktop_extra = {
      Keywords = "editor;text;code;",
    },
    linux_appstream = {
      keywords = "<keyword>editor</keyword><keyword>text</keyword>",
    },
  },
}
```

### Stripping rules

When `strip_dev_fields = true`, the following are removed from the shipped `coconut.config.json`:

- The whole `debug` block
- The whole `manifests` block
- `manifests` sub-fields inside `darwin`, `win`, `linux` platform configs

Preserved: `window_*`, `resizable`, `frameless`, `transparent`, `title`,
`initial_view`, `view_root`, `asset_root`, `command_root`, `output_dir`, `views`,
`app.*`, `icon.*`, `darwin.ns.*`, platform overrides.

---

## 5. Platform overrides — `darwin` / `win` / `linux`

Each platform block can override window style and merge app/manifest identity.

```lua
return {
  -- Window style overrides (optional, overrides top-level)
  frameless   = false,
  transparent = false,

  darwin = {
    frameless   = true,       -- macOS: frameless only on this platform
    transparent = true,       -- macOS: transparent only on this platform

    app = {
      name = "My App (Mac)",  -- Override display name on macOS
      id   = "com.example.mac",
    },
    bundle_identifier = "com.example.mac.app",  -- Explicit bundle ID (takes priority over app.id)

    ns = {
      notification_alert_style = "banner",
      usage_descriptions = {
        NSCameraUsageDescription = "Need camera",
      },
    },

    manifests = {
      darwin_info_plist_extra = { ... },
      darwin_entitlements = { ... },
    },
  },

  win = {
    transparent = false,       -- Reset transparent on Windows
    app = { name = "My App" },
    bundle_identifier = "MyAppCompany.MyApp",
    -- win has no `ns` — ignored
  },

  linux = {
    app = {
      id   = "my-app",
      name = "my-app",
    },
    -- linux has no `ns` — ignored
  },
}
```

### Merge semantics

| Field | Merge rule |
|---|---|
| `frameless`, `transparent` | Optional\<bool\>. If present in platform block → overrides top-level. If absent → top-level value used. |
| `app.*` | Deep merge. Platform values override shared values. Empty strings = no override. |
| `bundle_identifier` | Platform string. Takes priority over `app.id` for bundle ID resolution. |
| `ns` | Only meaningful on `darwin`. Ignored elsewhere. |
| `manifests.*` | Deep merge. Platform values override shared `manifests.*` values. |

### Bundle identifier resolution

```
1. darwin.bundle_identifier          (if non-empty)
2. darwin.app.id                     (if non-empty)
3. app.id                            (shared default)
4. "coconut-app"                     (fallback)
```

---

## 6. C++ types

```cpp
struct AppConfig {
  std::string name;
  std::string id;
  std::string version;
  std::string description;
  std::string category;
};

struct IconConfig {
  std::string source;
  std::string icns_path;
  std::string ico_path;
  std::string png_path;
};

struct NsConfig {
  std::string notification_alert_style;
  std::map<std::string, std::string> usage_descriptions;
};

struct ManifestsConfig {
  bool strip_dev_fields = true;
  bool bytecode_config = false;
  std::vector<std::string> target_archs;
  std::map<std::string, std::string> darwin_info_plist_extra;
  std::map<std::string, std::string> darwin_entitlements;
  std::map<std::string, std::string> linux_desktop_extra;
  std::map<std::string, std::string> linux_appstream;
};

struct PlatformConfig {
  std::optional<bool> frameless;
  std::optional<bool> transparent;
  AppConfig app;
  std::string bundle_identifier;
  NsConfig ns;
  ManifestsConfig manifests;
};
```

---

## 7. Config stripping

```cpp
Config stripConfig(const Config& cfg);
```

Returns a copy of `cfg` with dev-only fields zeroed:

- `debug` → `DebugConfig{}` (all false/empty)
- `manifests` → `ManifestsConfig{}` (all defaults)
- `darwin.manifests`, `win.manifests`, `linux.manifests` → reset to defaults

The stripped config is what ships in the bundle as `coconut.config.json`.
