#include "store.h"
#include "app.h"
#include "bridge.h"
#include "debug.h"
#include "../store.h"  // C++ store namespace (coconut::store)

namespace coconut::modules {

/// Helper: look up the App* from lua["coconut"]["_app"].
static App* getApp(sol::this_state s) {
  sol::state_view lua(s);
  sol::object appObj = lua["coconut"]["_app"];
  if (!appObj.valid() || !appObj.is<App*>()) return nullptr;
  return appObj.as<App*>();
}

void init_store(sol::state& lua, ThreadKind kind) {
  sol::table coconut = lua["coconut"].get_or_create<sol::table>();
  sol::table store_mod = lua.create_table();

  if (kind == ThreadKind::Main) {
    store_mod.set_function("set",
        [](const std::string& key, const std::string& value,
           sol::this_state s) {
          App* app = getApp(s);
          if (!app || !app->bridge_state || !app->bridge_state->store) {
            debug::warn("store.set: _app not wired or store is null");
            return;
          }
          store::set(app->bridge_state->store, key, value);

          // Emit store:update event to JS
          if (app->bridge_state->transport) {
            nlohmann::json payload = {{"key", key}, {"value", value}};
            bridge::emitToJS(app, "store:update", payload);
          }
        });

    store_mod.set_function("get",
        [](const std::string& key, sol::this_state s) -> sol::object {
          App* app = getApp(s);
          if (!app || !app->bridge_state || !app->bridge_state->store) {
            debug::warn("store.get: _app not wired or store is null");
            return sol::lua_nil;
          }
          auto result = store::get(app->bridge_state->store, key);
          if (result) {
            return sol::make_object(s.lua_state(), *result);
          }
          debug::warn(std::format("store.get: {}", result.error().message));
          return sol::lua_nil;
        });

    store_mod.set_function("has",
        [](const std::string& key, sol::this_state s) -> bool {
          App* app = getApp(s);
          if (!app || !app->bridge_state || !app->bridge_state->store) {
            debug::warn("store.has: _app not wired or store is null");
            return false;
          }
          return store::has(app->bridge_state->store, key);
        });

    store_mod.set_function("delete",
        [](const std::string& key, sol::this_state s) {
          App* app = getApp(s);
          if (!app || !app->bridge_state || !app->bridge_state->store) {
            debug::warn("store.delete: _app not wired or store is null");
            return;
          }
          store::remove(app->bridge_state->store, key);

          // Emit store:update event to JS
          if (app->bridge_state->transport) {
            nlohmann::json payload = {{"key", key}, {"value", nullptr}};
            bridge::emitToJS(app, "store:update", payload);
          }
        });

    store_mod.set_function("clear", [](sol::this_state s) {
      App* app = getApp(s);
      if (!app || !app->bridge_state || !app->bridge_state->store) {
        debug::warn("store.clear: _app not wired or store is null");
        return;
      }
      store::clear(app->bridge_state->store);

      // Emit store:update event to JS
      if (app->bridge_state->transport) {
        nlohmann::json payload = {{"key", ""}, {"value", nullptr}};
        bridge::emitToJS(app, "store:update", payload);
      }
    });

    store_mod.set_function("keys", [](sol::this_state s) -> sol::table {
      sol::state_view lua(s);
      sol::table result = lua.create_table();
      App* app = getApp(s);
      if (!app || !app->bridge_state || !app->bridge_state->store) {
        debug::warn("store.keys: _app not wired or store is null");
        return result;
      }
      auto keys_vec = store::keys(app->bridge_state->store);
      for (size_t i = 0; i < keys_vec.size(); ++i) {
        result[i + 1] = keys_vec[i];
      }
      return result;
    });
  } else {
    // Background — stubs (will become forwarding when Future system is added)
    store_mod.set_function("set", [](const std::string&, sol::object) -> bool {
      debug::warn("[bg] store.set requires the main thread");
      return false;
    });
    store_mod.set_function("get", [](const std::string&) -> sol::object {
      debug::warn("[bg] store.get requires the main thread");
      return sol::lua_nil;
    });
    store_mod.set_function("has", [](const std::string&) -> bool {
      debug::warn("[bg] store.has requires the main thread");
      return false;
    });
    store_mod.set_function("delete", [](const std::string&) -> bool {
      debug::warn("[bg] store.delete requires the main thread");
      return false;
    });
    store_mod.set_function("clear", []() -> bool {
      debug::warn("[bg] store.clear requires the main thread");
      return false;
    });
    store_mod.set_function("keys", [](sol::this_state s) -> sol::table {
      debug::warn("[bg] store.keys requires the main thread");
      sol::state_view lua(s);
      return lua.create_table();
    });
  }

  coconut["store"] = store_mod;
}

} // namespace coconut::modules
