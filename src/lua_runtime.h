#ifndef LUA_RUNTIME_H
#define LUA_RUNTIME_H

#include "config.h"
#include "context.h"

#include <lua.h>
#include <sol/sol.hpp>
#include <sol/state.hpp>

namespace coconut {
namespace lua {

struct Runtime {
  Config *configs = nullptr;
  CoconutContext *context = nullptr;
  sol::state *lua_state = nullptr;
};

Runtime *create(Config *config, CoconutContext *ctx);
void destroy(Runtime *runtime);

void _bindCoconut(Runtime *runtime);
void _bindViewClass(Runtime *runtime);
void _bindUserType(Runtime *runtime);

} // namespace lua
} // namespace coconut

#endif // LUA_RUNTIME_H
