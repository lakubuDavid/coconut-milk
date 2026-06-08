#include "bridge.h"
#include "app.h"
#include "lua_runtime.h"

extern "C" {
#include <webui.h>
}

#include <algorithm>
#include <format>
#include <string>

namespace coconut::bridge {

std::expected<State *, Error> create(Config *config) {
  return new State{.configs = config};
}

static std::string escapeLuaString(std::string_view s) {
  std::string out;
  out.reserve(s.size());
  for (char c : s) {
    switch (c) {
      case '\\':
        out += "\\\\";
        break;
      case '"':
        out += "\\\"";
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
        out += c;
        break;
    }
  }
  return out;
}

static std::string escapeJsSingleQuotedString(std::string_view s) {
  std::string out;
  out.reserve(s.size());
  for (char c : s) {
    switch (c) {
      case '\\':
        out += "\\\\";
        break;
      case '\'':
        out += "\\'";
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
        out += c;
        break;
    }
  }
  return out;
}

void emitToLua(coconut::App *app, std::string eventName,
               nlohmann::json payload) {
  if (app == nullptr || app->lua_state == nullptr || app->lua_state->lua_state == nullptr) {
    return;
  }

  // For now we pass payload as a JSON string.
  // Later versions should decode into a Lua table.
  const auto payloadJson = payload.dump();
  const auto luaEventName = escapeLuaString(eventName);
  const auto luaPayloadJson = escapeLuaString(payloadJson);

  const std::string script = std::format(
      R"(if coconut and coconut.events then coconut.events("{}", "{}", ctx) end)",
      luaEventName, luaPayloadJson);

  app->lua_state->lua_state->do_string(script);
}

void emitToJS(coconut::App *app, std::string eventName,
               nlohmann::json payload) {
  if (app == nullptr || app->window == nullptr) {
    return;
  }

  const auto payloadJson = payload.dump();
  const auto jsName = escapeJsSingleQuotedString(eventName);
  const auto jsPayloadJson = escapeJsSingleQuotedString(payloadJson);

  // JS side expects: __coconut_dispatch_event(name, payloadJsonString)
  const std::string script = std::format(
      "globalThis.__coconut_dispatch_event('{}', '{}');", jsName,
      jsPayloadJson);

  if (app->window->window_id > 0) {
    webui_run(app->window->window_id, script.c_str());
  }
}


void emitToJS(window::Window* window, std::string eventName,
              nlohmann::json payload) {
  if (window == nullptr) {
    return;
  }

  const auto payloadJson = payload.dump();
  const auto jsName = escapeJsSingleQuotedString(eventName);
  const auto jsPayloadJson = escapeJsSingleQuotedString(payloadJson);

  const std::string script = std::format(
      "globalThis.__coconut_dispatch_event('{}', '{}');", jsName,
      jsPayloadJson);

  if (window->window_id > 0) {
    webui_run(window->window_id, script.c_str());
  }
}

static sol::object jsonToLua(sol::state_view lua, const nlohmann::json& v) {
  if (v.is_null()) {
    return sol::lua_nil;
  }
  if (v.is_boolean()) {
    return sol::make_object(lua, v.get<bool>());
  }
  if (v.is_number_integer()) {
    return sol::make_object(lua, v.get<long long>());
  }
  if (v.is_number_unsigned()) {
    return sol::make_object(lua, v.get<unsigned long long>());
  }
  if (v.is_number_float()) {
    return sol::make_object(lua, v.get<double>());
  }
  if (v.is_string()) {
    return sol::make_object(lua, v.get<std::string>());
  }

  if (v.is_array()) {
    sol::table t = lua.create_table();
    std::size_t i = 1;
    for (const auto& item : v) {
      t[i++] = jsonToLua(lua, item);
    }
    return t;
  }

  if (v.is_object()) {
    sol::table t = lua.create_table();
    for (auto it = v.begin(); it != v.end(); ++it) {
      t[it.key()] = jsonToLua(lua, it.value());
    }
    return t;
  }

  return sol::lua_nil;
}

sol::table toTable(sol::state_view lua, const nlohmann::json& json) {
  sol::object obj = jsonToLua(lua, json);
  if (obj.is<sol::table>()) {
    return obj.as<sol::table>();
  }

  // Non-object/array: wrap as { value = ... } so we still return a table.
  sol::table t = lua.create_table();
  t["value"] = obj;
  return t;
}

sol::table toTable(sol::state_view lua, const std::string& jsonStr) {
  nlohmann::json parsed = nlohmann::json::parse(jsonStr);
  return toTable(lua, parsed);
}

static nlohmann::json luaToJsonValue(const sol::object& obj);

static nlohmann::json luaToJsonTable(const sol::table& t) {
  // Decide array vs object.
  bool looksArray = true;
  std::size_t maxIndex = 0;
  std::size_t count = 0;

  for (auto&& kv : t) {
    const sol::object& k = kv.first;
    if (!k.is<int>() && !k.is<long long>() && !k.is<unsigned int>() &&
        !k.is<unsigned long long>()) {
      looksArray = false;
      break;
    }

    std::size_t idx = static_cast<std::size_t>(k.as<long long>());
    maxIndex = std::max(maxIndex, idx);
    ++count;
  }

  if (looksArray && maxIndex >= count) {
    nlohmann::json arr = nlohmann::json::array();
    for (std::size_t i = 0; i < maxIndex; ++i) {
      arr.push_back(nullptr);
    }

    for (auto&& kv : t) {
      const sol::object& k = kv.first;
      std::size_t idx = static_cast<std::size_t>(k.as<long long>());
      const sol::object& v = kv.second;
      arr[idx - 1] = luaToJsonValue(v);
    }
    return arr;
  }

  nlohmann::json obj = nlohmann::json::object();
  for (auto&& kv : t) {
    const sol::object& k = kv.first;
    const sol::object& v = kv.second;

    std::string key;
    if (k.is<std::string>()) {
      key = k.as<std::string>();
    } else {
      // Fallback: stringify numeric keys.
      if (k.is<long long>() || k.is<int>()) {
        key = std::to_string(static_cast<long long>(k.as<long long>()));
      } else {
        key = "";
      }
    }

    if (!key.empty()) {
      obj[key] = luaToJsonValue(v);
    }
  }
  return obj;
}

static nlohmann::json luaToJsonValue(const sol::object& obj) {
  if (!obj.valid()) {
    return nullptr;
  }
  if (obj == sol::lua_nil) {
    return nullptr;
  }
  if (obj.is<bool>()) {
    return obj.as<bool>();
  }
  if (obj.is<std::string>()) {
    return obj.as<std::string>();
  }
  if (obj.is<int>()) {
    return obj.as<int>();
  }
  if (obj.is<long long>()) {
    return obj.as<long long>();
  }
  if (obj.is<double>()) {
    return obj.as<double>();
  }
  if (obj.is<float>()) {
    return static_cast<double>(obj.as<float>());
  }
  if (obj.is<sol::table>()) {
    return luaToJsonTable(obj.as<sol::table>());
  }

  // Fallback: encode as string.
  return obj.as<std::string>();
}

nlohmann::json toJson(const sol::table& table) {
  return luaToJsonTable(table);
}

void callLua(coconut::App *app, std::string functionName,
              nlohmann::json payload) {
  if (app == nullptr || app->lua_state == nullptr ||
      app->lua_state->lua_state == nullptr) {
    return;
  }

  sol::state &lua = *app->lua_state->lua_state;
  sol::protected_function fn = lua[functionName];
  if (!fn.valid()) {
    return;
  }

  // Convert JSON payload to a Lua table value.
  sol::object arg = functionName == "coconut.events"
                        ? sol::make_object(lua, payload.dump())
                        : toTable(lua, payload);

  auto result = fn(arg);
  if (!result.valid()) {
    sol::error err = result;
    (void)err;
  }
}

void callJS(coconut::App *app, std::string functionName,
            nlohmann::json payload) {
  if (app == nullptr || app->window == nullptr) {
    return;
  }
  if (app->window->window_id == 0) {
    return;
  }

  const std::string payloadStr = payload.dump();
  const std::string script = std::format(
      "globalThis['{}']({});", functionName, payloadStr);

  webui_run(app->window->window_id, script.c_str());
}

void dispatchEventToLua(webui_event_t* e) {
  if (e == nullptr) {
    return;
  }

  // The runtime is attached as user context in lua_runtime.cpp.
  void* raw = webui_get_context(e);
  auto* runtime = static_cast<coconut::lua::Runtime*>(raw);
  if (runtime == nullptr || runtime->lua_state == nullptr || runtime->context == nullptr) {
    return;
  }

  // JS contract: __coconut_emit(name: string, payloadJson: string)
  const char* eventNameC = webui_get_string(e);
  const char* payloadJsonC = webui_get_string_at(e, 1);

  const std::string eventName = eventNameC ? eventNameC : "";
  const std::string payloadJsonStr = payloadJsonC ? payloadJsonC : "{}";

  nlohmann::json payload;
  try {
    payload = nlohmann::json::parse(payloadJsonStr);
  } catch (...) {
    payload = nlohmann::json::object();
  }

  sol::state_view lua_view(*runtime->lua_state);

  sol::table payloadTable = toTable(lua_view, payload);

  sol::object coconutObj = (*runtime->lua_state)["coconut"];
  if (!coconutObj.valid() || !coconutObj.is<sol::table>()) {
    return;
  }

  sol::table coconutTbl = coconutObj.as<sol::table>();
  sol::protected_function eventsFn = coconutTbl["events"];
  if (!eventsFn.valid()) {
    return;
  }

  // Call: coconut.events(name, payloadTable, ctx)
  eventsFn(eventName, payloadTable, runtime->context);
}

void _coconut_js_listener(webui_event_t* e) {
  dispatchEventToLua(e);
}

void destroy(State *state) { delete state; }

} // namespace coconut::bridge
