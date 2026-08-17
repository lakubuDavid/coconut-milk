#include "config.h"
#include "error.h"

#include <nlohmann/json.hpp>

// getcwd(3)
#if defined(_WIN32)
#  include <direct.h>   // _getcwd
#  define getcwd _getcwd
#else
#  include <unistd.h>
#endif

#include <sol/state.hpp>
#include <sol/table.hpp>

#include <exception>
#include <fstream>
#include <string>
#include <string_view>

// ── Config path resolution ─────────────────────────────────────────────────

/// Resolve a relative path against the actual working directory (getcwd).
/// This avoids issues when the binary is launched through a symlink and
/// std::filesystem::current_path() resolves to the symlink directory.
static std::string resolveConfigPath(std::string_view relative) {
  if (relative.empty()) return {};
  if (relative[0] == '/') return std::string(relative);
  char cwd[4096];
  if (::getcwd(cwd, sizeof(cwd)) == nullptr) return std::string(relative);
  return std::string(cwd) + "/" + std::string(relative);
}

namespace coconut {

// ── Lua helpers ─────────────────────────────────────────────────────────────

/// Copy a scalar field from a sol::table_proxy if it exists and has the right type.
template <typename T>
static void luaCopy(const sol::table& src, const char* key, T& dst) {
  sol::object val = src[key];
  if (val.is<T>()) dst = val.as<T>();
}

// ── NsConfig parser (Lua) ──────────────────────────────────────────────────

static void parseNsConfig(const sol::table& t, NsConfig& cfg) {
  luaCopy(t, "notification_alert_style", cfg.notification_alert_style);

  {
    sol::object obj = t["usage_descriptions"];
    if (obj.is<sol::table>()) {
      sol::table m = obj.as<sol::table>();
      for (auto& [k, v] : m) {
        if (v.is<std::string>()) {
          cfg.usage_descriptions[k.as<std::string>()] = v.as<std::string>();
        }
      }
    }
  }
}

// ── AppConfig parser (Lua) ─────────────────────────────────────────────────

static void parseAppConfig(const sol::table& t, AppConfig& cfg) {
  luaCopy(t, "name",         cfg.name);
  luaCopy(t, "id",          cfg.id);
  luaCopy(t, "version",     cfg.version);
  luaCopy(t, "description", cfg.description);
  luaCopy(t, "category",    cfg.category);
}

// ── IconConfig parser (Lua) ─────────────────────────────────────────────────

static void parseIconConfig(const sol::table& t, IconConfig& cfg) {
  luaCopy(t, "source",    cfg.source);
  luaCopy(t, "icns_path", cfg.icns_path);
  luaCopy(t, "ico_path",  cfg.ico_path);
  luaCopy(t, "png_path",  cfg.png_path);
}

// ── ManifestsConfig parser (Lua) ─────────────────────────────────────────────

static void parseManifestsConfig(const sol::table& t, ManifestsConfig& cfg) {
  luaCopy(t, "strip_dev_fields", cfg.strip_dev_fields);
  luaCopy(t, "bytecode_config",  cfg.bytecode_config);

  // target_archs: array of strings
  {
    sol::object obj = t["target_archs"];
    if (obj.is<sol::table>()) {
      sol::table arr = obj.as<sol::table>();
      for (auto& [k, v] : arr) {
        if (v.is<std::string>()) {
          cfg.target_archs.push_back(v.as<std::string>());
        }
      }
    }
  }

  // darwin_info_plist_extra: table → string map
  {
    sol::object obj = t["darwin_info_plist_extra"];
    if (obj.is<sol::table>()) {
      sol::table m = obj.as<sol::table>();
      for (auto& [k, v] : m) {
        if (v.is<std::string>()) {
          cfg.darwin_info_plist_extra[k.as<std::string>()] = v.as<std::string>();
        }
      }
    }
  }

  // darwin_entitlements
  {
    sol::object obj = t["darwin_entitlements"];
    if (obj.is<sol::table>()) {
      sol::table m = obj.as<sol::table>();
      for (auto& [k, v] : m) {
        if (v.is<std::string>()) {
          cfg.darwin_entitlements[k.as<std::string>()] = v.as<std::string>();
        }
      }
    }
  }

  // linux_desktop_extra
  {
    sol::object obj = t["linux_desktop_extra"];
    if (obj.is<sol::table>()) {
      sol::table m = obj.as<sol::table>();
      for (auto& [k, v] : m) {
        if (v.is<std::string>()) {
          cfg.linux_desktop_extra[k.as<std::string>()] = v.as<std::string>();
        }
      }
    }
  }

  // linux_appstream
  {
    sol::object obj = t["linux_appstream"];
    if (obj.is<sol::table>()) {
      sol::table m = obj.as<sol::table>();
      for (auto& [k, v] : m) {
        if (v.is<std::string>()) {
          cfg.linux_appstream[k.as<std::string>()] = v.as<std::string>();
        }
      }
    }
  }
}

// ── PlatformConfig parser (Lua) ────────────────────────────────────────────

static void parsePlatformConfig(const sol::table& t, PlatformConfig& cfg) {
  // Window style scalars (replace if present)
  {
    sol::object obj = t["frameless"];
    if (obj.is<bool>()) cfg.frameless = obj.as<bool>();
  }
  {
    sol::object obj = t["transparent"];
    if (obj.is<bool>()) cfg.transparent = obj.as<bool>();
  }

  // app: deep-merged
  {
    sol::object obj = t["app"];
    if (obj.is<sol::table>()) {
      parseAppConfig(obj.as<sol::table>(), cfg.app);
    }
  }
  // bundle_identifier: explicit override for this platform
  luaCopy(t, "bundle_identifier", cfg.bundle_identifier);

  // ns: permission / notification strings
  {
    sol::object obj = t["ns"];
    if (obj.is<sol::table>()) {
      parseNsConfig(obj.as<sol::table>(), cfg.ns);
    }
  }

  // manifests: deep-merged
  {
    sol::object obj = t["manifests"];
    if (obj.is<sol::table>()) {
      parseManifestsConfig(obj.as<sol::table>(), cfg.manifests);
    }
  }
}

// ── JSON helpers ─────────────────────────────────────────────────────────────

/// Copy an int from nlohmann::json if it exists.
static void jsonCopyInt(const nlohmann::json& src, const char* key, int& dst) {
  if (src.contains(key) && src[key].is_number_integer()) dst = src[key].get<int>();
}

/// Copy a bool from nlohmann::json if it exists.
static void jsonCopyBool(const nlohmann::json& src, const char* key, bool& dst) {
  if (src.contains(key) && src[key].is_boolean()) dst = src[key].get<bool>();
}

/// Copy a string from nlohmann::json if it exists.
static void jsonCopyStr(const nlohmann::json& src, const char* key, std::string& dst) {
  if (src.contains(key) && src[key].is_string()) dst = src[key].get<std::string>();
}

// ── NsConfig parser (JSON) ───────────────────────────────────────────────


static void parseNsConfig(const nlohmann::json& j, NsConfig& cfg) {
  jsonCopyStr(j, "notification_alert_style", cfg.notification_alert_style);
  if (j.contains("usage_descriptions") && j["usage_descriptions"].is_object()) {
    for (auto& [k, v] : j["usage_descriptions"].items())
      if (v.is_string()) cfg.usage_descriptions[k] = v.get<std::string>();
  }
}

// ── AppConfig parser (JSON) ─────────────────────────────────────────────────


static void parseAppConfig(const nlohmann::json& j, AppConfig& cfg) {
  jsonCopyStr(j, "name",         cfg.name);
  jsonCopyStr(j, "id",          cfg.id);
  jsonCopyStr(j, "version",     cfg.version);
  jsonCopyStr(j, "description", cfg.description);
  jsonCopyStr(j, "category",    cfg.category);
}

// ── IconConfig parser (JSON) ─────────────────────────────────────────────────

static void parseIconConfig(const nlohmann::json& j, IconConfig& cfg) {
  jsonCopyStr(j, "source",    cfg.source);
  jsonCopyStr(j, "icns_path", cfg.icns_path);
  jsonCopyStr(j, "ico_path",  cfg.ico_path);
  jsonCopyStr(j, "png_path",  cfg.png_path);
}

// ── ManifestsConfig parser (JSON) ───────────────────────────────────────────

static void parseManifestsConfig(const nlohmann::json& j, ManifestsConfig& cfg) {
  jsonCopyBool(j, "strip_dev_fields", cfg.strip_dev_fields);
  jsonCopyBool(j, "bytecode_config",  cfg.bytecode_config);

  // target_archs: array of strings
  if (j.contains("target_archs") && j["target_archs"].is_array()) {
    for (const auto& a : j["target_archs"]) {
      if (a.is_string()) cfg.target_archs.push_back(a.get<std::string>());
    }
  }

  if (j.contains("darwin_info_plist_extra") && j["darwin_info_plist_extra"].is_object()) {
    for (auto& [k, v] : j["darwin_info_plist_extra"].items())
      if (v.is_string()) cfg.darwin_info_plist_extra[k] = v.get<std::string>();
  }
  if (j.contains("darwin_entitlements") && j["darwin_entitlements"].is_object()) {
    for (auto& [k, v] : j["darwin_entitlements"].items())
      if (v.is_string()) cfg.darwin_entitlements[k] = v.get<std::string>();
  }
  if (j.contains("linux_desktop_extra") && j["linux_desktop_extra"].is_object()) {
    for (auto& [k, v] : j["linux_desktop_extra"].items())
      if (v.is_string()) cfg.linux_desktop_extra[k] = v.get<std::string>();
  }
  if (j.contains("linux_appstream") && j["linux_appstream"].is_object()) {
    for (auto& [k, v] : j["linux_appstream"].items())
      if (v.is_string()) cfg.linux_appstream[k] = v.get<std::string>();
  }
}

// ── PlatformConfig parser (JSON) ─────────────────────────────────────────────

static void parsePlatformConfig(const nlohmann::json& j, PlatformConfig& cfg) {
  if (j.contains("frameless") && j["frameless"].is_boolean())
    cfg.frameless = j["frameless"].get<bool>();
  if (j.contains("transparent") && j["transparent"].is_boolean())
    cfg.transparent = j["transparent"].get<bool>();
  if (j.contains("app") && j["app"].is_object())
    parseAppConfig(j["app"], cfg.app);
  jsonCopyStr(j, "bundle_identifier", cfg.bundle_identifier);
  if (j.contains("ns") && j["ns"].is_object())
    parseNsConfig(j["ns"], cfg.ns);
  if (j.contains("manifests") && j["manifests"].is_object())
    parseManifestsConfig(j["manifests"], cfg.manifests);
}

// ── JSON loader ─────────────────────────────────────────────────────────────

std::expected<Config, Error>
loadConfigJson(std::string_view config_path) {
  std::string abs_path = resolveConfigPath(config_path);
  std::ifstream f{abs_path};
  if (!f.is_open()) {
    return std::unexpected(Error{.code = ErrorCode::MissingFile,
                                 .message = "failed to open config file",
                                 .details = abs_path});
  }

  try {
    nlohmann::json j = nlohmann::json::parse(f);
    Config cfg{};

    // Global scalars
    // Global scalars — validate types first
    auto checkType = [&j](const char* key, bool is_bool) -> std::expected<void, Error> {
      if (!j.contains(key)) return {};
      if (is_bool) {
        if (!j[key].is_boolean())
          return std::unexpected(Error{ErrorCode::InvalidConfig,
            std::string(key) + " must be a boolean"});
      } else {
        if (!j[key].is_number_integer() && !j[key].is_string())
          return std::unexpected(Error{ErrorCode::InvalidConfig,
            std::string(key) + " has an invalid type"});
      }
      return {};
    };
    {
      auto e = checkType("frameless", true);
      if (!e) return std::unexpected(e.error());
    }
    {
      auto e = checkType("transparent", true);
      if (!e) return std::unexpected(e.error());
    }
    {
      auto e = checkType("resizable", true);
      if (!e) return std::unexpected(e.error());
    }
    {
      auto e = checkType("window_width", false);
      if (!e) return std::unexpected(e.error());
    }
    {
      auto e = checkType("window_height", false);
      if (!e) return std::unexpected(e.error());
    }

    jsonCopyInt(j, "window_width",      cfg.window_width);
    jsonCopyInt(j, "window_height",     cfg.window_height);
    jsonCopyInt(j, "window_min_width",  cfg.window_min_width);
    jsonCopyInt(j, "window_min_height", cfg.window_min_height);
    jsonCopyInt(j, "window_max_width",  cfg.window_max_width);
    jsonCopyInt(j, "window_max_height", cfg.window_max_height);
    jsonCopyBool(j, "resizable",        cfg.resizable);
    jsonCopyBool(j, "frameless",        cfg.frameless);
    jsonCopyBool(j, "transparent",       cfg.transparent);
    jsonCopyStr(j, "title",            cfg.title);
    jsonCopyStr(j, "initial_view",     cfg.initial_view);
    jsonCopyStr(j, "view_root",        cfg.view_root);
    jsonCopyStr(j, "fallback_file",    cfg.fallback_file);
    jsonCopyStr(j, "asset_root",       cfg.asset_root);
    jsonCopyStr(j, "command_root",     cfg.command_root);
    jsonCopyStr(j, "output_dir",       cfg.output_dir);

    if (j.contains("debug")) {
      if (j["debug"].is_boolean()) {
        cfg.debug.enabled = j["debug"].get<bool>();
        cfg.debug.showTransportDump = cfg.debug.enabled;
      } else if (j["debug"].is_object()) {
        cfg.debug.enabled = j["debug"].value("enabled", false);
        cfg.debug.showTransportDump = j["debug"].value("showTransportDump", false);
        jsonCopyStr(j["debug"], "logLevel", cfg.debug.logLevel);
      }
    }

    // app
    if (j.contains("app") && j["app"].is_object())
      parseAppConfig(j["app"], cfg.app);

    // icon
    if (j.contains("icon") && j["icon"].is_object())
      parseIconConfig(j["icon"], cfg.icon);

    // manifests
    if (j.contains("manifests") && j["manifests"].is_object())
      parseManifestsConfig(j["manifests"], cfg.manifests);

    // darwin / win / linux
    if (j.contains("darwin") && j["darwin"].is_object())
      parsePlatformConfig(j["darwin"], cfg.darwin);
    if (j.contains("win") && j["win"].is_object())
      parsePlatformConfig(j["win"], cfg.win);
    if (j.contains("linux") && j["linux"].is_object())
      parsePlatformConfig(j["linux"], cfg.linux);

    // views
    if (j.contains("views") && j["views"].is_object()) {
      for (auto& [name, v] : j["views"].items()) {
        if (!v.is_object()) continue;
        if (!v.contains("kind") || !v["kind"].is_string())
          return std::unexpected(Error{ErrorCode::InvalidConfig,
            "view '" + name + "' is missing 'kind' or 'kind' is not a string"});
        if (!v.contains("src") || !v["src"].is_string())
          return std::unexpected(Error{ErrorCode::InvalidConfig,
            "view '" + name + "' is missing 'src' or 'src' is not a string"});
        std::string kind = v["kind"].get<std::string>();
        if (kind != "file" && kind != "html" && kind != "url")
          return std::unexpected(Error{ErrorCode::InvalidConfig,
            "view '" + name + "' has invalid kind '" + kind + "'"});
        cfg.views[name] = ViewEntry{kind, v["src"].get<std::string>()};
      }
    }

    return cfg;
  } catch (const std::exception& e) {
    return std::unexpected(Error{.code = ErrorCode::ParseError,
                                .message = "failed to parse config json",
                                .details = e.what()});
  }
}

// ── Lua loader ───────────────────────────────────────────────────────────────

std::expected<Config, Error>
loadConfigLua(std::string_view config_path) {
  std::string abs_path = resolveConfigPath(config_path);
  {
    std::ifstream probe{abs_path};
    if (!probe.is_open()) {
      return std::unexpected(Error{.code = ErrorCode::MissingFile,
                                   .message = "failed to open config file",
                                   .details = abs_path});
    }
  }

  try {
    sol::state lua;
    lua.open_libraries(sol::lib::jit,sol::lib::base, sol::lib::table, sol::lib::os);

    auto result = lua.safe_script_file(abs_path,
                                       sol::script_pass_on_error);
    if (!result.valid()) {
      sol::error err = result;
      return std::unexpected(Error{.code = ErrorCode::ParseError,
                                   .message = "failed to run lua config",
                                   .details = err.what()});
    }

    if (result.get_type() != sol::type::table) {
      return std::unexpected(Error{.code = ErrorCode::InvalidConfig,
                                   .message = "lua config must return a table"});
    }

    sol::table t = result;
    Config cfg{};

    // Global scalar fields
    cfg.window_width      = t["window_width"].get_or(1280);
    cfg.window_height     = t["window_height"].get_or(640);
    cfg.window_min_width  = t["window_min_width"].get_or(0);
    cfg.window_min_height = t["window_min_height"].get_or(0);
    cfg.window_max_width  = t["window_max_width"].get_or(0);
    cfg.window_max_height = t["window_max_height"].get_or(0);
    cfg.resizable         = t["resizable"].get_or(true);
    cfg.frameless         = t["frameless"].get_or(false);
    cfg.transparent       = t["transparent"].get_or(false);
    cfg.title             = t["title"].get_or<std::string>("Coconut");
    cfg.initial_view      = t["initial_view"].get_or<std::string>("home");
    cfg.view_root         = t["view_root"].get_or<std::string>("views");
    cfg.fallback_file     = t["fallback_file"].get_or<std::string>("");
    cfg.asset_root        = t["asset_root"].get_or<std::string>("assets");
    cfg.command_root       = t["command_root"].get_or<std::string>("commands");
    cfg.output_dir        = t["output_dir"].get_or<std::string>(cfg.output_dir);

    // debug
    {
      sol::object debugObj = t["debug"];
      if (debugObj.is<bool>()) {
        cfg.debug.enabled = debugObj.as<bool>();
        cfg.debug.showTransportDump = cfg.debug.enabled;
      } else if (debugObj.is<sol::table>()) {
        sol::table dt = debugObj.as<sol::table>();
        cfg.debug.enabled = dt["enabled"].get_or(false);
        cfg.debug.showTransportDump = dt["showTransportDump"].get_or(false);
        cfg.debug.logLevel = dt["logLevel"].get_or<std::string>("info");
      }
    }

    // hmr (hot module replacement)
    {
      sol::object hmrObj = t["hmr"];
      if (hmrObj.is<sol::table>()) {
        sol::table ht = hmrObj.as<sol::table>();
        cfg.hmr.enabled = ht["enabled"].get_or(false);
        cfg.hmr.auto_regenerate = ht["auto_regenerate"].get_or(false);
      }
    }

    // generators.output_dir (backward compat)
    {
      sol::object gen = t["generators"];
      if (gen.is<sol::table>()) {
        cfg.output_dir = gen.as<sol::table>()["output_dir"].get_or<std::string>(cfg.output_dir);
      }
    }

    // app
    {
      sol::object obj = t["app"];
      if (obj.is<sol::table>()) parseAppConfig(obj.as<sol::table>(), cfg.app);
    }

    // icon
    {
      sol::object obj = t["icon"];
      if (obj.is<sol::table>()) parseIconConfig(obj.as<sol::table>(), cfg.icon);
    }

    // manifests
    {
      sol::object obj = t["manifests"];
      if (obj.is<sol::table>()) parseManifestsConfig(obj.as<sol::table>(), cfg.manifests);
    }

    // darwin
    {
      sol::object obj = t["darwin"];
      if (obj.is<sol::table>()) parsePlatformConfig(obj.as<sol::table>(), cfg.darwin);
    }

    // win
    {
      sol::object obj = t["win"];
      if (obj.is<sol::table>()) parsePlatformConfig(obj.as<sol::table>(), cfg.win);
    }

    // linux
    {
      sol::object obj = t["linux"];
      if (obj.is<sol::table>()) parsePlatformConfig(obj.as<sol::table>(), cfg.linux);
    }

    // views
    {
      sol::object views_obj = t["views"];
      if (views_obj.is<sol::table>()) {
        sol::table views = views_obj.as<sol::table>();
        for (auto& [key, value] : views) {
          if (!value.is<sol::table>()) continue;
          sol::table vt = value.as<sol::table>();
          std::string name = key.as<std::string>();
          std::string kind = vt["kind"].get_or<std::string>("");
          if (kind != "file" && kind != "html" && kind != "url") continue;
          cfg.views[name] = ViewEntry{std::move(kind), vt["src"].get_or<std::string>("")};
        }
      }
    }

    return cfg;
  } catch (const std::exception& e) {
    return std::unexpected(Error{.code = ErrorCode::LuaError,
                                .message = "lua config error",
                                .details = e.what()});
  }
}

// ── Composite loader ─────────────────────────────────────────────────────────

std::expected<Config, Error>
loadConfig(std::string_view lua_path, std::string_view json_path) {
  auto result = loadConfigLua(lua_path);
  if (result) return result;
  if (result.error().code == ErrorCode::MissingFile)
    return loadConfigJson(json_path);
  return result;
}

// ── Config stripping for bundle ─────────────────────────────────────────────

/// Strip dev fields from a single PlatformConfig (manifests sub-block).
static void stripPlatformConfig(PlatformConfig& p) {
  p.manifests = ManifestsConfig{};  // reset to defaults (all empty/off)
}

Config stripConfig(const Config& cfg) {
  Config out = cfg;

  // Strip debug block (dev-only)
  out.debug = DebugConfig{};

  // Strip manifests block (dev-time bundling config)
  out.manifests = ManifestsConfig{};

  // Strip manifests sub-blocks from platform configs
  stripPlatformConfig(out.darwin);
  stripPlatformConfig(out.win);
  stripPlatformConfig(out.linux);

  return out;
}

} // namespace coconut
