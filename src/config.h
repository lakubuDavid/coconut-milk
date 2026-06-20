#ifndef CONFIG_H
#define CONFIG_H

#include <expected>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "error.h"

namespace coconut {

// ── App identity (shared cross-platform defaults) ────────────────────────────

/// Shared app identity fields.
/// These are defaults used by all platforms unless overridden per-platform.
struct AppConfig {
  std::string name;         ///< Display name (used in menus, window titles, etc.)
  std::string id;           ///< Conceptual app identifier (used as default CFBundleIdentifier, AppUserModelID, .desktop id)
  std::string version;      ///< Version string (semver, e.g. "1.0.0")
  std::string description;  ///< Short description for desktop entries
  std::string category;     ///< High-level category (maps to per-platform taxonomy)
};

/// Icon references per platform.
/// Icon formats differ per OS so these are kept platform-specific.
/// If `source` is set, the bundle command auto-generates platform-specific
/// icon files from a single source file (SVG, PNG, JPEG, ICNS, or ICO).
/// The explicit *_path fields override / complement auto-generation.
struct IconConfig {
  std::string source;     ///< Single source file — SVG, PNG, JPEG, ICNS, or ICO
  std::string icns_path;  ///< macOS: path to .icns file
  std::string ico_path;    ///< Windows: path to .ico file
  std::string png_path;    ///< Linux: path to .png file (or freedesktop icon name)
};

// ── macOS system permission / notification strings ───────────────────────────

/// macOS NS*UsageDescription strings for system permission prompts.
/// Maps key name (e.g. "NSCameraUsageDescription") → description text.
struct NsConfig {
  std::string notification_alert_style;  ///< NSUserNotificationAlertStyle (alert|banner|none)
  std::map<std::string, std::string> usage_descriptions;  ///< NS*UsageDescription keys
};

// ── Bundling / packaging hints ─────────────────────────────────────────────

/// Bundling configuration (dev-time config, not runtime).
struct ManifestsConfig {
  /// Strip dev-only fields (generators, debug, manifests.*) from config
  /// before shipping in the bundle. Default: true.
  bool strip_dev_fields = true;

  /// Compile the stripped config to .luac bytecode (B2 opt-in).
  /// User must bundle separately per arch if using this.
  bool bytecode_config = false;

  /// Target architectures for multi-arch bundling.
  /// e.g. {"x86_64", "arm64"} for fat binaries.
  /// If empty, only the current host arch is bundled.
  std::vector<std::string> target_archs;

  /// Extra raw fields merged into the generated Info.plist (macOS).
  std::map<std::string, std::string> darwin_info_plist_extra;

  /// Entitlements plist content (macOS).
  std::map<std::string, std::string> darwin_entitlements;

  /// Extra raw fields merged into the generated .desktop file (Linux).
  std::map<std::string, std::string> linux_desktop_extra;

  /// AppStream metainfo sections (Linux).
  std::map<std::string, std::string> linux_appstream;
};

// ── Platform-specific overrides ───────────────────────────────────────────────

/// Per-platform overrides for window style and manifest identity.
///
/// Merge semantics:
///   - Scalars (frameless, transparent, window_*) → replaced if present in platform block
///   - Tables (app, manifests) → deep-merged (platform values override shared values)
struct PlatformConfig {
  // Window style overrides (optional per-platform)
  std::optional<bool> frameless;
  std::optional<bool> transparent;

  // App identity overrides for this platform (deep-merged with shared app.*)
  AppConfig app;

  // Explicit bundle identifier for this platform (overrides app.id)
  std::string bundle_identifier;

  // macOS permission / notification strings
  NsConfig ns;

  // Manifests for this platform (deep-merged with shared manifests.*)
  ManifestsConfig manifests;
};

// ── Debug config ───────────────────────────────────────────────────────────

/// Debug settings — off by default.
struct DebugConfig {
  bool enabled           = false;  ///< Master switch (CLI --debug, or config)
  bool showTransportDump = false;  ///< Log bridge message payloads
  std::string logLevel   = "info"; ///< Minimum log level: debug|info|warn|error
};

// ── HMR (Hot Module Replacement) config ────────────────────────────────────

/// HMR settings — off by default, auto-enabled in --debug mode.
struct HmrConfig {
  bool enabled          = false;  ///< Master switch
  bool auto_regenerate  = false;  ///< Run `coconut generate` on file changes
};

// ── View entry ─────────────────────────────────────────────────────────────

/// Describes a view as declared in the startup config file.
///
/// At load time the framework converts these into runtime `window::View`
/// objects by resolving the `src` (reading file content for file-based views,
/// storing inline HTML for html-based views, etc.).
struct ViewEntry {
  std::string kind;  ///< "file", "html", or "url"
  std::string src;   ///< file path, inline HTML string, or URL
};

// ── Main Config ─────────────────────────────────────────────────────────────

/// Shared startup configuration.
///
/// Created once at startup (with defaults) and distributed as `const Config*`
/// to all runtime modules.  Some fields (window_size, initial_view)
/// may be mutated by `coconut.config(ctx)` during the startup hook.
struct Config {
  int window_width = 1280;
  int window_height = 640;
  int window_min_width = 0;
  int window_min_height = 0;
  int window_max_width = 0;
  int window_max_height = 0;
  bool resizable = true;
  bool frameless = false;
  bool transparent = false;
  DebugConfig debug;
  HmrConfig hmr;
  std::string title = "Coconut";
  std::string initial_view = "home";
  std::string view_root = "views";
  std::string fallback_file;          ///< SPA fallback: serve this file on 404 (relative to project root)
  std::string asset_root = "assets";
  std::string command_root = "commands";
  std::string output_dir = "generated";
  std::map<std::string, ViewEntry> views;

  // ── App identity (shared cross-platform defaults) ──────────────────────
  AppConfig app;
  IconConfig icon;

  // ── Bundling / packaging hints ─────────────────────────────────────────
  ManifestsConfig manifests;

  // ── Platform-specific overrides ─────────────────────────────────────────
  PlatformConfig darwin;
  PlatformConfig win;
  PlatformConfig linux;
};

/// Load startup configuration from a JSON file.
///
/// Unknown keys are ignored.
/// Missing keys keep the compiled defaults.
std::expected<Config, Error>
loadConfigJson(std::string_view config_path = "coconut.config.json");

/// Load startup configuration from a Lua file that returns a table.
std::expected<Config, Error>
loadConfigLua(std::string_view config_path = "coconut.config.lua");

/// Try loading config from Lua first; fall back to JSON on MissingFile.
///
/// Order: `coconut.config.lua` → `coconut.config.json` → C++ defaults.
std::expected<Config, Error>
loadConfig(std::string_view lua_path = "coconut.config.lua",
           std::string_view json_path = "coconut.config.json");

/// Strip dev-only fields from a Config for shipping in a bundle.
///
/// Removes the following fields for the bundle version:
///   - debug (whole block)
///   - manifests (whole block — dev-time config, not runtime)
///
/// Platform config sub-blocks are preserved but their manifests
/// sub-fields are stripped.
///
/// This is Option A (default). Option B2 (opt-in) additionally
/// compiles the stripped config to Lua bytecode.
Config stripConfig(const Config& cfg);

} // namespace coconut

#endif // CONFIG_H
