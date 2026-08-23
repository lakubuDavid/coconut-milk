#include "json.h"

namespace coconut::modules {

  // ---------------------------------------------------------------------------
  // Module registration
  // ---------------------------------------------------------------------------

  void init_json(sol::state& lua, ThreadKind kind) {
    (void)kind;  // thread-safe

    sol::table coconut  = lua["coconut"].get_or_create<sol::table>();
    sol::table json_mod = lua.create_table();

    json_mod.set_function("jsonify", [](sol::object obj) -> std::string {
      if (!obj.valid() || obj.get_type() == sol::type::lua_nil)
        return "null";
      if (obj.get_type() != sol::type::table)
        return "{}";
      auto json = toJson(obj.as<sol::table>());
      return json.dump();
    });

    json_mod.set_function("parse", [&lua](const std::string& str) -> sol::object {
      try {
        auto json = nlohmann::json::parse(str);
        return toTable(lua, json);
      } catch (const std::exception&) {
        return sol::make_object(lua, sol::lua_nil);
      }
    });

    coconut["json"] = json_mod;
  }

}  // namespace coconut::modules
