#include "config.h"
#include "error.h"

#include <nlohmann/json.hpp>

#include <sol/state.hpp>
#include <sol/table.hpp>

#include <exception>
#include <fstream>
#include <string>
#include <string_view>

namespace coconut {

// ── JSON loader ──────────────────────────────────────────────────────────────

std::expected<Config, Error>
loadConfigJson(std::string_view config_path) {
  std::ifstream f{std::string(config_path)};
  if (!f.is_open()) {
    return std::unexpected(Error{.code = ErrorCode::MissingFile,
                                 .message = "failed to open config file",
                                 .details = std::string(config_path)});
  }

  try {
    nlohmann::json j = nlohmann::json::parse(f);

    Config cfg{};

    // --- scalar fields ---

    if (j.contains("browser") && !j["browser"].is_null()) {
      if (!j["browser"].is_string()) {
        return std::unexpected(
            Error{.code = ErrorCode::InvalidConfig,
                  .message = "config.browser must be a string",
                  .details = j["browser"].dump()});
      }
      cfg.browser = j["browser"].get<std::string>();
    }

    if (j.contains("window_width") && !j["window_width"].is_null()) {
      if (!j["window_width"].is_number_integer()) {
        return std::unexpected(
            Error{.code = ErrorCode::InvalidConfig,
                  .message = "config.window_width must be an integer",
                  .details = j["window_width"].dump()});
      }
      cfg.window_width = j["window_width"].get<int>();
    }

    if (j.contains("window_height") && !j["window_height"].is_null()) {
      if (!j["window_height"].is_number_integer()) {
        return std::unexpected(
            Error{.code = ErrorCode::InvalidConfig,
                  .message = "config.window_height must be an integer",
                  .details = j["window_height"].dump()});
      }
      cfg.window_height = j["window_height"].get<int>();
    }

    if (j.contains("window_min_width") && !j["window_min_width"].is_null()) {
      if (!j["window_min_width"].is_number_integer()) {
        return std::unexpected(
            Error{.code = ErrorCode::InvalidConfig,
                  .message = "config.window_min_width must be an integer",
                  .details = j["window_min_width"].dump()});
      }
      cfg.window_min_width = j["window_min_width"].get<int>();
    }

    if (j.contains("window_min_height") && !j["window_min_height"].is_null()) {
      if (!j["window_min_height"].is_number_integer()) {
        return std::unexpected(
            Error{.code = ErrorCode::InvalidConfig,
                  .message = "config.window_min_height must be an integer",
                  .details = j["window_min_height"].dump()});
      }
      cfg.window_min_height = j["window_min_height"].get<int>();
    }

    if (j.contains("window_max_width") && !j["window_max_width"].is_null()) {
      if (!j["window_max_width"].is_number_integer()) {
        return std::unexpected(
            Error{.code = ErrorCode::InvalidConfig,
                  .message = "config.window_max_width must be an integer",
                  .details = j["window_max_width"].dump()});
      }
      cfg.window_max_width = j["window_max_width"].get<int>();
    }

    if (j.contains("window_max_height") && !j["window_max_height"].is_null()) {
      if (!j["window_max_height"].is_number_integer()) {
        return std::unexpected(
            Error{.code = ErrorCode::InvalidConfig,
                  .message = "config.window_max_height must be an integer",
                  .details = j["window_max_height"].dump()});
      }
      cfg.window_max_height = j["window_max_height"].get<int>();
    }

    if (j.contains("resizable")) {
      if (!j["resizable"].is_boolean()) {
        return std::unexpected(
            Error{.code = ErrorCode::InvalidConfig,
                  .message = "config.resizable must be a boolean",
                  .details = j["resizable"].dump()});
      }
      cfg.resizable = j["resizable"].get<bool>();
    }

    if (j.contains("initial_view") && !j["initial_view"].is_null()) {
      if (!j["initial_view"].is_string()) {
        return std::unexpected(
            Error{.code = ErrorCode::InvalidConfig,
                  .message = "config.initial_view must be a string",
                  .details = j["initial_view"].dump()});
      }
      cfg.initial_view = j["initial_view"].get<std::string>();
    }

    if (j.contains("view_root") && !j["view_root"].is_null()) {
      if (!j["view_root"].is_string()) {
        return std::unexpected(
            Error{.code = ErrorCode::InvalidConfig,
                  .message = "config.view_root must be a string",
                  .details = j["view_root"].dump()});
      }
      cfg.view_root = j["view_root"].get<std::string>();
    }

    if (j.contains("asset_root") && !j["asset_root"].is_null()) {
      if (!j["asset_root"].is_string()) {
        return std::unexpected(
            Error{.code = ErrorCode::InvalidConfig,
                  .message = "config.asset_root must be a string",
                  .details = j["asset_root"].dump()});
      }
      cfg.asset_root = j["asset_root"].get<std::string>();
    }

    if (j.contains("command_root") && !j["command_root"].is_null()) {
      if (!j["command_root"].is_string()) {
        return std::unexpected(
            Error{.code = ErrorCode::InvalidConfig,
                  .message = "config.command_root must be a string",
                  .details = j["command_root"].dump()});
      }
      cfg.command_root = j["command_root"].get<std::string>();
    }

    // --- views block (optional) ---

    if (j.contains("views") && !j["views"].is_null()) {
      if (!j["views"].is_object()) {
        return std::unexpected(
            Error{.code = ErrorCode::InvalidConfig,
                  .message = "config.views must be an object",
                  .details = j["views"].dump()});
      }

      for (auto& [name, v] : j["views"].items()) {
        if (!v.is_object()) {
          return std::unexpected(
              Error{.code = ErrorCode::InvalidConfig,
                    .message = "config.views entry must be an object",
                    .details = v.dump()});
        }

        // Each view entry needs "kind" and "src"
        if (!v.contains("kind") || !v["kind"].is_string()) {
          return std::unexpected(Error{
              .code = ErrorCode::InvalidConfig,
              .message = "config.views." + name + " must have a string 'kind' field",
              .details = v.dump()});
        }
        if (!v.contains("src") || !v["src"].is_string()) {
          return std::unexpected(Error{
              .code = ErrorCode::InvalidConfig,
              .message = "config.views." + name + " must have a string 'src' field",
              .details = v.dump()});
        }

        std::string kind = v["kind"].get<std::string>();
        if (kind != "file" && kind != "html" && kind != "url") {
          return std::unexpected(Error{
              .code = ErrorCode::InvalidConfig,
              .message =
                  "config.views." + name + ".kind must be 'file', 'html', or 'url'",
              .details = v.dump()});
        }

        cfg.views[name] = ViewEntry{
            .kind = std::move(kind),
            .src = v["src"].get<std::string>(),
        };
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
  // Check file existence first for a clear error message.
  {
    std::ifstream probe{std::string(config_path)};
    if (!probe.is_open()) {
      return std::unexpected(Error{.code = ErrorCode::MissingFile,
                                   .message = "failed to open config file",
                                   .details = std::string(config_path)});
    }
  }

  try {
    sol::state lua;
    lua.open_libraries(sol::lib::base, sol::lib::table);

    auto result = lua.safe_script_file(std::string(config_path),
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

    // Scalar fields with defaults
    cfg.browser = t["browser"].get_or<std::string>("auto");
    cfg.window_width = t["window_width"].get_or(1280);
    cfg.window_height = t["window_height"].get_or(640);
    cfg.window_min_width = t["window_min_width"].get_or(0);
    cfg.window_min_height = t["window_min_height"].get_or(0);
    cfg.window_max_width = t["window_max_width"].get_or(0);
    cfg.window_max_height = t["window_max_height"].get_or(0);
    cfg.resizable = t["resizable"].get_or(true);
    cfg.initial_view = t["initial_view"].get_or<std::string>("home");
    cfg.view_root = t["view_root"].get_or<std::string>("views");
    cfg.asset_root = t["asset_root"].get_or<std::string>("assets");
    cfg.command_root = t["command_root"].get_or<std::string>("commands");

    // Views block (optional)
    sol::object views_obj = t["views"];
    if (views_obj.is<sol::table>()) {
      sol::table views = views_obj;
      for (auto& [key, value] : views) {
        if (!value.is<sol::table>()) continue;
        sol::table vt = value;
        std::string name = key.as<std::string>();
        std::string kind = vt["kind"].get_or<std::string>("");
        if (kind != "file" && kind != "html" && kind != "url") continue;
        cfg.views[name] = ViewEntry{
            .kind = std::move(kind),
            .src = vt["src"].get_or<std::string>(""),
        };
      }
    }

    return cfg;
  } catch (const std::exception& e) {
    return std::unexpected(Error{.code = ErrorCode::LuaError,
                                 .message = "lua config error",
                                 .details = e.what()});
  }
}

// ── Composite loader (Lua → JSON → defaults) ────────────────────────────────

std::expected<Config, Error>
loadConfig(std::string_view lua_path, std::string_view json_path) {
  auto result = loadConfigLua(lua_path);
  if (result) return result;

  // Only fall back to JSON if the Lua file was simply missing.
  if (result.error().code == ErrorCode::MissingFile) {
    return loadConfigJson(json_path);
  }

  // Propagate any real error (parse failure, invalid config, etc.)
  return result;
}

} // namespace coconut
