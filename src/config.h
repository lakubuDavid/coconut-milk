#ifndef CONFIG_H
#define CONFIG_H

#include <expected>
#include <map>
#include <string>
#include <string_view>

#include "error.h"

namespace coconut {

/// Describes a view as declared in the startup config file.
///
/// At load time the framework converts these into runtime `window::View`
/// objects by resolving the `src` (reading file content for file-based views,
/// storing inline HTML for html-based views, etc.).
struct ViewEntry {
  std::string kind;  /// "file", "html", or "url"
  std::string src;   /// file path, inline HTML string, or URL
};

/// Shared startup configuration.
///
/// Created once at startup (with defaults) and distributed as `const Config*`
/// to all runtime modules.  Some fields (browser, window_size, initial_view)
/// may be mutated by `coconut.config(ctx)` during the startup hook.
struct Config {
  std::string browser = "auto";
  int window_width = 1280;
  int window_height = 640;
  int window_min_width = 0;
  int window_min_height = 0;
  int window_max_width = 0;
  int window_max_height = 0;
  bool resizable = true;
  bool frameless = false;
  bool transparent = false;
  std::string title = "Coconut";
  std::string initial_view = "home";
  std::string view_root = "views";
  std::string asset_root = "assets";
  std::string command_root = "commands";
  std::map<std::string, ViewEntry> views;
};

/// Load startup configuration from a JSON file.
///
/// Unknown keys are ignored.
/// Missing keys keep the compiled defaults.
///
/// Expected JSON shape (example):
/// ```json
/// {
///   "browser": "auto",
///   "window_width": 1280,
///   "window_height": 640,
///   "window_min_width": 0,
///   "window_min_height": 0,
///   "window_max_width": 0,
///   "window_max_height": 0,
///   "resizable": true,
///   "frameless": false,
///   "transparent": false,
///   "title": "Coconut",
///   "initial_view": "home",
///   "view_root": "views",
///   "asset_root": "assets",
///   "command_root": "commands",
///   "views": {
///     "home":  { "kind": "file", "src": "views/home.html" },
///     "note":  { "kind": "file", "src": "views/note.html" },
///     "about": { "kind": "html", "src": "<h1>About</h1>" }
///   }
/// }
/// ```
std::expected<Config, Error>
loadConfigJson(std::string_view config_path = "coconut.config.json");

/// Load startup configuration from a Lua file that returns a table.
///
/// Expected Lua shape (example):
/// ```lua
/// return {
///   browser = "auto",
///   window_width = 1280,
///   window_height = 640,
///   window_min_width = 0,
///   window_min_height = 0,
///   window_max_width = 0,
///   window_max_height = 0,
///   resizable = true,
///   frameless = false,
///   transparent = false,
///   title = "Coconut",
///   initial_view = "home",
///   view_root = "views",
///   asset_root = "assets",
///   command_root = "commands",
///   views = {
///     home = { kind = "file", src = "views/home.html" },
///   },
///   generators = {
///     output_dir = "generated"
///   }
/// }
/// ```
std::expected<Config, Error>
loadConfigLua(std::string_view config_path = "coconut.config.lua");

/// Try loading config from Lua first; fall back to JSON on MissingFile.
///
/// Order: `coconut.config.lua` → `coconut.config.json` → C++ defaults.
std::expected<Config, Error>
loadConfig(std::string_view lua_path = "coconut.config.lua",
           std::string_view json_path = "coconut.config.json");

} // namespace coconut

#endif // CONFIG_H
