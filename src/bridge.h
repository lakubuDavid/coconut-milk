#ifndef BRIDGE_H
#define BRIDGE_H

#include "config.h"
#include "error.h"
extern "C" {
  #include "webui.h"
}
#include "window.h"

#include <nlohmann/json.hpp>

#include <expected>

#include <sol/sol.hpp>
#include <sol/table.hpp>

namespace coconut {
  class App;
  namespace bridge {

    struct State {
      Config* configs = nullptr;
    };

    std::expected<State*, Error> create(Config* config);
    void                         destroy(State* state);

    void emitToLua(coconut::App* app,std::string eventName, nlohmann::json payload);
    void emitToJS(window::Window* window,std::string eventName,nlohmann::json payload);
    void emitToJS(coconut::App* app,std::string eventName,nlohmann::json payload);

    void callLua(coconut::App* app,std::string fuctionName,nlohmann::json payload);
    void callJS(coconut::App* app,std::string functionName,nlohmann::json payload);

    /// Register the __coconut_emit JS callback and inject the JS adapter.
    /// Encapsulates the webui_bind / set_context / run calls that were
    /// previously in lua_runtime.cpp.
    void setupEmitBinding(App* app);

    void _coconut_js_listener(webui_event_t* e);

    void dispatchEventToLua(webui_event_t* e);

    // Convert json object/array to a lua table.
    // Requires a Lua state to allocate the sol::table.
    sol::table toTable(sol::state_view lua, const nlohmann::json& json);
    sol::table toTable(sol::state_view lua, const std::string& json);

    // Convert lua table to a json value.
    nlohmann::json toJson(const sol::table& table);

  }  // namespace bridge
}  // namespace coconut

#endif  // BRIDGE_H
