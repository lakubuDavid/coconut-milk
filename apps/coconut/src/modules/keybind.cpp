// SPDX-License-Identifier: MIT
#include "keybind.h"
#include "app.h"
#include "debug.h"

#include <format>
#include <string>
// #include <utility>

namespace coconut::modules {

  void init_keybind(sol::state& lua, ThreadKind kind) {
    sol::table coconut = lua["coconut"].get_or_create<sol::table>();

    if (kind != ThreadKind::Main) {
      // Background thread stubs.
      coconut.set_function(
          "keybind",
          [](sol::this_state,
             const std::string&,
             sol::protected_function,
             sol::optional<sol::table>) -> sol::function {
            debug::warn("[bg] coconut.keybind requires the main thread");
            return sol::lua_nil;
          }
      );
      coconut.set_function("__registerPlatformKeybind", [](sol::table) -> bool {
        debug::warn("[bg] __registerPlatformKeybind requires the main thread");
        return false;
      });
      return;
    }

    // ── Platform keybinds ───────────────────────────────────────────
    coconut.set_function(
        "__registerPlatformKeybind", [](sol::this_state s, sol::table params) -> bool {
          sol::state_view lv(s);
          sol::object     appObj = lv["coconut"]["_app"];
          if (!appObj.valid() || !appObj.is<App*>())
            return false;
          App* app = appObj.as<App*>();

          std::string combo = params["combo"].get_or<std::string>("");
          if (combo.empty())
            return false;
          app->platform_keybinds.insert(combo);
          debug::info(std::format("[keybind] registered platform keybind: {}", combo));
          return true;
        }
    );

    // ── Keybind system (hybrid chain) ────────────────────────────────
    coconut["_keybinds"] = lua.create_table();

    coconut.set_function(
        "keybind",
        [](sol::this_state           s,
           const std::string&        combo,
           sol::protected_function   handler,
           sol::optional<sol::table> opts_tbl) -> sol::function {
          sol::state_view lv(s);
          sol::table      coconut = lv["coconut"];

          // Build entry table.
          std::string id = opts_tbl ? opts_tbl.value()["id"].get_or(combo) : combo;
          std::string scope =
              opts_tbl ? opts_tbl.value()["scope"].get_or(std::string("global")) : "global";
          bool platform = opts_tbl ? opts_tbl.value()["platform"].get_or(false) : false;

          // If platform-level, register with App's platform_keybinds set.
          if (platform) {
            sol::object appObj = coconut["_app"];
            if (appObj.valid() && appObj.is<App*>()) {
              App* app = appObj.as<App*>();
              app->platform_keybinds.insert(combo);
              debug::info(std::format("[keybind] registered platform keybind: {}", combo));
            }
          }

          // Store in coconut._keybinds[combo] list.
          sol::table keybinds = coconut["_keybinds"];
          sol::table list     = keybinds[combo];
          if (!list.valid()) {
            list            = lv.create_table();
            keybinds[combo] = list;
          }
          list[list.size() + 1] = handler;

          // Also store metadata under a parallel table for lookup.
          sol::table meta = coconut["_keybind_meta"];
          if (!meta.valid()) {
            meta                     = lv.create_table();
            coconut["_keybind_meta"] = meta;
          }
          sol::table meta_entry  = lv.create_table();
          meta_entry["id"]       = id;
          meta_entry["combo"]    = combo;
          meta_entry["scope"]    = scope;
          meta_entry["platform"] = platform;
          meta[id]               = meta_entry;

          // Return unregister closure.
          return lv.script(R"(
          return function()
            -- no-op placeholder; actual unregister uses
            -- __coconut_unregister_keybind(combo, id)
          end
        )");
        }
    );

    // ── Lua-side cleanup helper ──────────────────────────────────────
    sol::state_view lv(lua);
    lv.script(R"(
    if not __coconut_unregister_keybind then
      __coconut_unregister_keybind = function(combo, id)
        local c = coconut
        if c._keybinds and c._keybinds[combo] then
          c._keybinds[combo] = nil
        end
        if c._keybind_meta and c._keybind_meta[id] then
          c._keybind_meta[id] = nil
        end
      end
    end
  )");
  }

}  // namespace coconut::modules
