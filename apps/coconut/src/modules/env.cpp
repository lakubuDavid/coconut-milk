#include "env.h"
#include "../packages/env.h"  // C++ env namespace

namespace coconut::modules {

void init_env(sol::state& lua, ThreadKind kind) {
  if (kind == ThreadKind::Background) return;  // not available on bg

  sol::table coconut = lua["coconut"].get_or_create<sol::table>();
  sol::table env_tbl = lua.create_table();
  sol::table mt = lua.create_table();

  mt["__index"] = [&lua](sol::table, const std::string& key) -> sol::object {
    sol::state_view lv(lua);
    if (key == "cwd") {
      return sol::make_object(lv.lua_state(), coconut::env::cwd());
    }
    if (key == "homedir") {
      return sol::make_object(lv.lua_state(), coconut::env::homedir());
    }
    if (key == "pathSeparator") {
      return sol::make_object(lv.lua_state(), std::string(1, coconut::env::pathSeparator()));
    }
    std::string val = coconut::env::get(key);
    if (val.empty()) return sol::nil;
    return sol::make_object(lv.lua_state(), val);
  };

  env_tbl[sol::metatable_key] = mt;
  coconut["env"] = env_tbl;
}

} // namespace coconut::modules
