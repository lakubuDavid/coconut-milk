#include "common.h"

#include <nlohmann/json.hpp>
#include <sol/sol.hpp>

#include <cstddef>
#include <string>

namespace coconut::common {

  // ── Escape ──────────────────────────────────────────────────────────

  std::string escapeString(std::string_view s, char quote_char) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
      switch (c) {
        case '\\':
          out += "\\\\";
          break;
        case '\n':
          out += "\\n";
          break;
        case '\r':
          out += "\\r";
          break;
        case '\t':
          out += "\\t";
          break;
        default:
          if (c == quote_char)
            out += '\\';
          out += c;
          break;
      }
    }
    return out;
  }

  // ── JSON ↔ Lua ─────────────────────────────────────────────────────

  static std::string sanitizeJsonString(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size();) {
      unsigned char c = s[i];
      if (c == 0x00) {
        ++i;
        continue;
      }
      if (c < 0x80) {
        out.push_back(c);
        ++i;
        continue;
      }
      if ((c & 0xE0) == 0xC0 && i + 1 < s.size() && (s[i + 1] & 0xC0) == 0x80) {
        out.push_back(c);
        out.push_back(s[i + 1]);
        i += 2;
        continue;
      }
      if ((c & 0xF0) == 0xE0 && i + 2 < s.size() && (s[i + 1] & 0xC0) == 0x80 &&
          (s[i + 2] & 0xC0) == 0x80) {
        out.push_back(c);
        out.push_back(s[i + 1]);
        out.push_back(s[i + 2]);
        i += 3;
        continue;
      }
      if ((c & 0xF8) == 0xF0 && i + 3 < s.size() && (s[i + 1] & 0xC0) == 0x80 &&
          (s[i + 2] & 0xC0) == 0x80 && (s[i + 3] & 0xC0) == 0x80) {
        out.push_back(c);
        out.push_back(s[i + 1]);
        out.push_back(s[i + 2]);
        out.push_back(s[i + 3]);
        i += 4;
        continue;
      }
      ++i;
    }
    return out;
  }

  static nlohmann::json luaToJsonValue(const sol::object& obj);

  static nlohmann::json luaToJsonTable(const sol::table& t) {
    bool        looksArray = true;
    std::size_t maxIndex   = 0;
    std::size_t count      = 0;
    for (auto&& kv : t) {
      const sol::object& k = kv.first;
      if (!k.is<int>() && !k.is<long long>() && !k.is<unsigned int>() &&
          !k.is<unsigned long long>()) {
        looksArray = false;
        break;
      }
      std::size_t idx = static_cast<std::size_t>(k.as<long long>());
      maxIndex        = (std::max)(maxIndex, idx);
      ++count;
    }
    if (looksArray && maxIndex >= count) {
      nlohmann::json arr = nlohmann::json::array();
      for (std::size_t i = 0; i < maxIndex; ++i) arr.push_back(nullptr);
      for (auto&& kv : t) {
        std::size_t idx = static_cast<std::size_t>(kv.first.as<long long>());
        arr[idx - 1]    = luaToJsonValue(kv.second);
      }
      return arr;
    }
    nlohmann::json obj = nlohmann::json::object();
    for (auto&& kv : t) {
      const sol::object& k = kv.first;
      std::string        key;
      if (k.is<std::string>())
        key = k.as<std::string>();
      else if (k.is<long long>() || k.is<int>())
        key = std::to_string(static_cast<long long>(k.as<long long>()));
      else
        key = "";
      if (!key.empty())
        obj[key] = luaToJsonValue(kv.second);
    }
    return obj;
  }

  static nlohmann::json luaToJsonValue(const sol::object& obj) {
    if (!obj.valid())
      return nullptr;
    if (obj == sol::lua_nil)
      return nullptr;
    if (obj.is<bool>())
      return obj.as<bool>();
    if (obj.is<std::string>())
      return sanitizeJsonString(obj.as<std::string>());
    if (obj.is<int>())
      return obj.as<int>();
    if (obj.is<long long>())
      return obj.as<long long>();
    if (obj.is<double>())
      return obj.as<double>();
    if (obj.is<float>())
      return static_cast<double>(obj.as<float>());
    if (obj.is<sol::table>())
      return luaToJsonTable(obj.as<sol::table>());
    return sanitizeJsonString(obj.as<std::string>());
  }

  static sol::object jsonToLua(sol::state_view lua, const nlohmann::json& v) {
    if (v.is_null())
      return sol::lua_nil;
    if (v.is_boolean())
      return sol::make_object(lua, v.get<bool>());
    if (v.is_number_integer())
      return sol::make_object(lua, v.get<long long>());
    if (v.is_number_unsigned())
      return sol::make_object(lua, v.get<unsigned long long>());
    if (v.is_number_float())
      return sol::make_object(lua, v.get<double>());
    if (v.is_string())
      return sol::make_object(lua, v.get<std::string>());
    if (v.is_array()) {
      sol::table  t = lua.create_table();
      std::size_t i = 1;
      for (const auto& item : v) t[i++] = jsonToLua(lua, item);
      return t;
    }
    if (v.is_object()) {
      sol::table t = lua.create_table();
      for (auto it = v.begin(); it != v.end(); ++it) t[it.key()] = jsonToLua(lua, it.value());
      return t;
    }
    return sol::lua_nil;
  }

  sol::table toTable(sol::state_view lua, const nlohmann::json& json) {
    sol::object obj = jsonToLua(lua, json);
    if (obj.is<sol::table>())
      return obj.as<sol::table>();
    sol::table t = lua.create_table();
    t["value"]   = obj;
    return t;
  }

  sol::table toTable(sol::state_view lua, const std::string& jsonStr) {
    return toTable(lua, nlohmann::json::parse(jsonStr));
  }

  nlohmann::json toJson(const sol::table& table) {
    return luaToJsonTable(table);
  }

}  // namespace coconut::common
