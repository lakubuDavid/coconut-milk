#include "store.h"
#include <cstddef>
#include <format>
#include <nlohmann/json_fwd.hpp>
#include <optional>
#include <sol/forward.hpp>
#include <sol/object.hpp>
#include <sol/state.hpp>
#include <sol/state_view.hpp>
#include <sol/table.hpp>
#include <sol/types.hpp>
#include <string>
#include <vector>
#include "../store.h"  // C++ store namespace (coconut::store)
#include "app.h"
#include "bridge.h"
#include "debug.h"
#include "forward.h"
#include "modules/thread_kind.h"

namespace coconut::modules {

  namespace {
    App* g_storeApp = nullptr;  ///< forwarding target for Background ops
  }

  void setStoreApp(App* app) {
    g_storeApp = app;
  }

  /// Helper: look up the App* from lua["coconut"]["_app"].
  static App* getApp(sol::this_state s) {
    sol::state_view lua(s);
    sol::object     appObj = lua["coconut"]["_app"];
    if (!appObj.valid() || !appObj.is<App*>())
      return nullptr;
    return appObj.as<App*>();
  }

  void init_store(sol::state& lua, ThreadKind kind) {
    sol::table coconut   = lua["coconut"].get_or_create<sol::table>();
    sol::table store_mod = lua.create_table();

    if (kind == ThreadKind::Main) {
      store_mod.set_function(
          "set",
          [](const std::string& key, const std::string& value, sol::this_state s) {
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
          }
      );

      store_mod.set_function("get", [](const std::string& key, sol::this_state s) -> sol::object {
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

      store_mod.set_function("has", [](const std::string& key, sol::this_state s) -> bool {
        App* app = getApp(s);
        if (!app || !app->bridge_state || !app->bridge_state->store) {
          debug::warn("store.has: _app not wired or store is null");
          return false;
        }
        return store::has(app->bridge_state->store, key);
      });

      store_mod.set_function("delete", [](const std::string& key, sol::this_state s) {
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
        sol::table      result = lua.create_table();
        App*            app    = getApp(s);
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
      // Background — forward onto the main run loop via dispatch::post and
      // block until the operation completes. The closure re-resolves the
      // store from the injected App* (main-thread resources only).
      auto resolveApp = []() -> App* {
        if (!g_storeApp || !g_storeApp->bridge_state || !g_storeApp->bridge_state->store) {
          debug::warn("[bg] store: forwarding target not wired");
          return nullptr;
        }
        return g_storeApp;
      };

      store_mod.set_function("set", [resolveApp](const std::string& key, const std::string& value) {
        forwardToMain([&]() -> void {
          App* app = resolveApp();
          if (!app)
            return;
          store::set(app->bridge_state->store, key, value);
          if (app->bridge_state->transport) {
            nlohmann::json payload = {{"key", key}, {"value", value}};
            bridge::emitToJS(app, "store:update", payload);
          }
        });
      });

      store_mod.set_function(
          "get",
          [resolveApp](const std::string& key, sol::this_state s) -> sol::object {
            auto result = forwardToMain([&]() -> std::optional<std::string> {
              App* app = resolveApp();
              if (!app)
                return std::nullopt;
              auto value = store::get(app->bridge_state->store, key);
              if (value.has_value())
                return *value;
              return std::nullopt;
            });
            if (result) {
              return sol::make_object(s.lua_state(), *result);
            }
            return sol::lua_nil;
          }
      );

      store_mod.set_function("has", [resolveApp](const std::string& key) -> bool {
        return forwardToMain([&]() -> bool {
          App* app = resolveApp();
          return app ? store::has(app->bridge_state->store, key) : false;
        });
      });

      store_mod.set_function("delete", [resolveApp](const std::string& key) {
        forwardToMain([&]() -> void {
          App* app = resolveApp();
          if (!app)
            return;
          store::remove(app->bridge_state->store, key);
          if (app->bridge_state->transport) {
            nlohmann::json payload = {{"key", key}, {"value", nullptr}};
            bridge::emitToJS(app, "store:update", payload);
          }
        });
      });

      store_mod.set_function("clear", [resolveApp]() {
        forwardToMain([&]() -> void {
          App* app = resolveApp();
          if (!app)
            return;
          store::clear(app->bridge_state->store);
        });
      });

      store_mod.set_function("keys", [resolveApp](sol::this_state s) -> sol::table {
        auto            keyList = forwardToMain([&]() -> std::vector<std::string> {
          App* app = resolveApp();
          return app ? store::keys(app->bridge_state->store) : std::vector<std::string>{};
        });
        sol::state_view lua(s);
        sol::table      result = lua.create_table();
        for (size_t i = 0; i < keyList.size(); ++i) result[i + 1] = keyList[i];
        return result;
      });
    }

    coconut["store"] = store_mod;
  }

}  // namespace coconut::modules
