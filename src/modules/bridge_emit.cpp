// SPDX-License-Identifier: MIT
#include "bridge_emit.h"
#include "debug.h"
#include "bridge.h"
#include "app.h"

#include <nlohmann/json.hpp>

#include <format>
#include <string>

namespace coconut::modules {

void init_bridge_emit(sol::state& lua, ThreadKind kind) {
  sol::table coconut = lua["coconut"].get_or_create<sol::table>();

  if (kind != ThreadKind::Main) {
    // Background thread stub — _bridge_emit requires the main thread
    // to access the bridge/webview.
    coconut.set_function("_bridge_emit",
        [](const std::string&, const std::string&) {
          debug::warn("[bg] _bridge_emit requires the main thread");
        });
    coconut.set_function("_js_log",
        [](const sol::table&) {
          // JS log forwarding is main-thread only.
        });
    return;
  }

  // ── Low-level bridge helper: forwards to JS ───────────────────
  coconut.set_function(
      "_bridge_emit", [](sol::this_state s, const std::string& name,
                           const std::string& payloadJson) {
        // App* is looked up at call time via sol::this_state.
        sol::state_view lv(s);
        sol::object appObj = lv["coconut"]["_app"];
        if (!appObj.is<App*>()) return;
        App* app = appObj.as<App*>();

        try {
          auto json = nlohmann::json::parse(payloadJson);
          bridge::emitToJS(app, name, json);
        } catch (const std::exception& e) {
          debug::warn(std::format("_bridge_emit: failed to parse payload: {}",
                                  e.what()));
        }
      });

  // ── JS error forwarding ─────────────────────────────────────
  coconut.set_function("_js_log",
      [](sol::this_state, const sol::table& entry) {
        std::string level = entry.get_or<std::string>("level", "error");
        std::string message = entry.get_or<std::string>("message", "");
        std::string stack = entry.get_or<std::string>("stack", "");
        std::string full = message;
        if (!stack.empty()) full += "\n" + stack;
        if (level == "info") debug::info(full);
        else if (level == "warn") debug::warn(full);
        else debug::error(full);
      });
}

} // namespace coconut::modules
