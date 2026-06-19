#include "lua_runtime.h"

#include "app.h"
#include "bridge.h"
#include "debug.h"
#include "dialog.h"
#include "fs.h"
#include "packages/env.h"
#include "packages/open_url.h"
#include "packages/clipboard.h"
#include "packages/notify.h"

#include <sol/state.hpp>
#include <sol/table.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>

// Some system headers (ObjC runtime transitives) define `nil` as a macro
// which clashes with sol::type::nil and other uses. Undefine it here.
#ifdef nil
#undef nil
#endif

namespace coconut::lua {

std::expected<Runtime *, Error> create(Config *cfg, CoconutContext *ctx) {
  auto runtime =
      new Runtime{.configs = cfg, .context = ctx, .lua_state = nullptr};

  runtime->lua_state = new sol::state();
  runtime->lua_state->open_libraries(
      sol::lib::jit,
      sol::lib::base, sol::lib::package, sol::lib::io, sol::lib::os,
      sol::lib::table, sol::lib::string, sol::lib::math);

  _bindCoconutLuaApi(runtime);
  _bindViewClass(runtime);
  _bindUserType(runtime);
  _registerBuiltinCommands(runtime);

  // Transport is created by main.cpp after runtime->app is wired.

  return runtime;
}

void _registerBuiltinCommands(Runtime *runtime) {
  // Register framework-level commands so the JS bridge can dispatch to them.
  // These wrap the C++ functions bound on the coconut table.
  sol::state& lua = *runtime->lua_state;
  const char* src = R"(
    local ctx = _G.ctx
    if not ctx then return end

    ctx:bind("clipboard_read", function()
      return coconut.clipboard.readText()
    end)
    ctx:bind("clipboard_write", function(params)
      return coconut.clipboard.writeText(params.text or "")
    end)
    ctx:bind("openUrl", function(params)
      return coconut.openUrl(params.url or "")
    end)
    ctx:bind("notify", function(params)
      return coconut.notify(params.title or "", params.body or "")
    end)
    ctx:bind("dialog_message", function(params)
      return coconut.dialog.message(params.message or "",
                                     params.title or "Message",
                                     params.kind or "info")
    end)
    ctx:bind("dialog_open", function(params)
      return coconut.dialog.open(params.title or "Open",
                                  params.multi,
                                  params.chooseDir)
    end)
    ctx:bind("dialog_save", function(params)
      return coconut.dialog.save(params.title or "Save",
                                  params.defaultName or "")
    end)
    ctx:bind("fs_exists", function(params)
      local ok, exists = pcall(coconut.fs.exists, params.path)
      if ok then return { ok = true, exists = exists } end
      return { ok = false, error = tostring(exists) }
    end)
    ctx:bind("fs_write_text", function(params)
      local ok, err = pcall(coconut.fs.writeText, params.path, params.content)
      if ok then return { ok = err } end
      return { ok = false, error = tostring(err) }
    end)
    ctx:bind("fs_resolve", function(params)
      local ok, resolved = pcall(coconut.fs.resolve, params.root, params.relpath)
      if ok then return { ok = true, data = resolved } end
      return { ok = false, error = tostring(resolved) }
    end)
    ctx:bind("fs_list_dir", function(params)
      local ok, entries = pcall(coconut.fs.listDir, params.path)
      if ok then return { ok = true, data = entries } end
      return { ok = false, error = tostring(entries) }
    end)
    ctx:bind("ping", function()
      return "pong"
    end)
    ctx:bind("getViews", function()
      local names, i = {}, 1
      for name in pairs(coconut.views()) do
        names[i] = name; i = i + 1
      end
      return names
    end)
    ctx:bind("fs_read_text", function(params)
      local ok, data = pcall(coconut.fs.readText, params.path)
      if ok then return { ok = true, data = data } end
      return { ok = false, error = tostring(data) }
    end)
    ctx:bind("__coconutWindowCtl", function(params)
      local w = _coconut_window
      if not w then return { ok = false, error = "no window handle" } end
      local cmd = params.cmd
      if cmd == "minimize" then
        w:minimize()
      elseif cmd == "maximize" then
        w:maximize()
      elseif cmd == "close" then
        w:close()
      elseif cmd == "fullscreen_on" then
        w:setFullscreen(true)
      elseif cmd == "fullscreen_off" then
        w:setFullscreen(false)
      elseif cmd == "resize" then
        w:resize(params.w or 800, params.h or 600)
      elseif cmd == "setPosition" then
        w:setPosition(params.x or 0, params.y or 0)
      elseif cmd == "reload" then
        w:reload()
      end
      return { ok = true }
    end)
    ctx:bind("__registerPlatformKeybind", function(params)
      local combo = params.combo
      if combo and coconut.__registerPlatformKeybind then
        local ok = pcall(coconut.__registerPlatformKeybind, params)
        return { ok = ok }
      end
      return { ok = false, error = "missing combo or binding" }
    end)
    ctx:bind("store_set", function(params)
      local ok, err = pcall(coconut.store.set, params.key, params.value)
      if ok then return { ok = true } end
      return { ok = false, error = tostring(err) }
    end)
    ctx:bind("store_get", function(params)
      local ok, value = pcall(coconut.store.get, params.key)
      if ok then return { ok = true, value = value } end
      return { ok = false, error = tostring(value) }
    end)
    ctx:bind("store_has", function(params)
      local ok, has = pcall(coconut.store.has, params.key)
      if ok then return { ok = true, has = has } end
      return { ok = false, error = tostring(has) }
    end)
    ctx:bind("store_delete", function(params)
      local ok, err = pcall(coconut.store.delete, params.key)
      if ok then return { ok = true } end
      return { ok = false, error = tostring(err) }
    end)
    ctx:bind("store_clear", function()
      local ok, err = pcall(coconut.store.clear)
      if ok then return { ok = true } end
      return { ok = false, error = tostring(err) }
    end)
    ctx:bind("store_keys", function()
      local ok, keys = pcall(coconut.store.keys)
      if ok then return { ok = true, keys = keys } end
      return { ok = false, error = tostring(keys) }
    end)
  )";

  auto result = lua.script(src, sol::script_pass_on_error);
  if (!result.valid()) {
    sol::error err = result;
    debug::warn(std::format("builtin commands: {}", err.what()));
  }
}

void _bindCoconutLuaApi(Runtime *runtime) {
  sol::table coconut =
      (*runtime->lua_state)["coconut"].get_or_create<sol::table>();

  // Low-level bridge helper: forwards to JS (called from Lua coconut.emit)
  coconut.set_function(
      "_bridge_emit", [runtime](const std::string &name,
                                 const std::string &payloadJson) {
        if (runtime == nullptr || runtime->app == nullptr) return;
        try {
          auto json = nlohmann::json::parse(payloadJson);
          bridge::emitToJS(runtime->app, name, json);
        } catch (const std::exception &e) {
          debug::warn(std::format("_bridge_emit: failed to parse payload: {}",
                                  e.what()));
        }
      });

  // Debug logging functions exposed to Lua as coconut.log / .info / .warn / .error
  coconut.set_function("log",   [](const std::string& msg) { debug::log(msg); });
  coconut.set_function("info",  [](const std::string& msg) { debug::info(msg); });
  coconut.set_function("warn",  [](const std::string& msg) { debug::warn(msg); });
  coconut.set_function("error", [](const std::string& msg) { debug::error(msg); });

  // Built-in stubs — overridden by user's main.lua when loaded.
  coconut.set_function("config",
      [](CoconutContext* ctx) -> CoconutContext* { return ctx; });
  coconut.set_function("views",
      [](sol::this_state s) -> sol::table {
        return sol::state_view(s).create_table();
      });
  coconut.set_function("events",
      [](sol::object) { });

  // Register a combo with the platform-level keybind set (for NSEvent monitor)
  coconut.set_function("__registerPlatformKeybind", [runtime](sol::table params) -> bool {
    if (runtime && runtime->app) {
      std::string combo = params["combo"].get_or<std::string>("");
      if (combo.empty()) return false;
      runtime->app->platform_keybinds.insert(combo);
      debug::info(std::format("[keybind] registered platform keybind: {}", combo));
      return true;
    }
    return false;
  });

  // ── Dialog bindings: coconut.dialog ─────────────────────────────
  // Exposes native message box and file dialogs to Lua.
  sol::table dialog = (*runtime->lua_state).create_table();

  dialog.set_function("message", [](sol::variadic_args va) -> sol::table {
    sol::state_view lua = va.lua_state();
    std::string title = "Message";
    std::string message;
    std::string kind = "info";
    if (va.size() >= 1 && va[0].is<std::string>()) message = va[0].as<std::string>();
    if (va.size() >= 2 && va[1].is<std::string>()) title = va[1].as<std::string>();
    if (va.size() >= 3 && va[2].is<std::string>()) kind = va[2].as<std::string>();
    auto r = dialog::messageBox(title, message, kind);
    sol::table t = lua.create_table();
    t["confirmed"] = r.confirmed;
    return t;
  });

  dialog.set_function("open", [](sol::variadic_args va) -> sol::table {
    sol::state_view lua = va.lua_state();
    std::string title = "Open File";
    bool multi = false;
    bool chooseDir = false;
    std::vector<dialog::Filter> filters;
    if (va.size() >= 1 && va[0].is<std::string>()) title = va[0].as<std::string>();
    if (va.size() >= 2 && va[1].is<bool>()) multi = va[1].as<bool>();
    if (va.size() >= 3 && va[2].is<bool>()) chooseDir = va[2].as<bool>();
    auto r = dialog::openFile(title, filters, multi, chooseDir);
    sol::table t = lua.create_table();
    t["confirmed"] = r.confirmed;
    t["path"] = r.path;
    t["is_dir"] = r.is_dir;
    sol::table paths = lua.create_table();
    for (size_t i = 0; i < r.paths.size(); ++i) paths[i + 1] = r.paths[i];
    t["paths"] = paths;
    return t;
  });

  dialog.set_function("save", [](sol::variadic_args va) -> sol::table {
    sol::state_view lua = va.lua_state();
    std::string title = "Save File";
    std::string defaultName;
    if (va.size() >= 1 && va[0].is<std::string>()) title = va[0].as<std::string>();
    if (va.size() >= 2 && va[1].is<std::string>()) defaultName = va[1].as<std::string>();
    auto r = dialog::saveFile(title, defaultName);
    sol::table t = lua.create_table();
    t["confirmed"] = r.confirmed;
    t["path"] = r.path;
    return t;
  });

  coconut["dialog"] = dialog;

  // ── JSON utilities: coconut.json ──────────────────────────────
  // Provides jsonify (Lua table → JSON string) and parse (string → table).
  // Uses nlohmann::json under the hood via the bridge's conversion helpers.
  sol::table json_mod = (*runtime->lua_state).create_table();

  json_mod.set_function("jsonify",
      [](sol::object obj) -> std::string {
        if (!obj.valid() || obj.get_type() == sol::type::lua_nil) return "null";
        if (obj.get_type() != sol::type::table) return "{}";
        auto json = bridge::toJson(obj.as<sol::table>());
        return json.dump();
      });

  json_mod.set_function("parse",
      [runtime](const std::string& str) -> sol::object {
        sol::state_view lua(*runtime->lua_state);
        try {
          auto json = nlohmann::json::parse(str);
          return bridge::toTable(lua, json);
        } catch (const std::exception&) {
          return sol::make_object(lua, sol::lua_nil);
        }
      });

  coconut["json"] = json_mod;

  // ── Filesystem: coconut.fs ─────────────────────────────────────
  // Exposes readText, readBytes, writeText, writeBytes, exists, resolve.
  sol::table fs_mod = (*runtime->lua_state).create_table();

  fs_mod.set_function("readText",
      [](const std::string& path) -> std::string {
        auto result = fs::readText(path);
        if (result) return std::move(*result);
        debug::warn(std::format("fs.readText: {} ({})",
                     result.error().message, path));
        return {};
      });

  // Lua strings are byte-safe, so readBytes returns a Lua string too.
  fs_mod.set_function("readBytes",
      [](const std::string& path) -> std::string {
        auto result = fs::readBytes(path);
        if (result) {
          auto& vec = *result;
          return std::string(
              reinterpret_cast<const char*>(vec.data()), vec.size());
        }
        debug::warn(std::format("fs.readBytes: {} ({})",
                     result.error().message, path));
        return {};
      });

  fs_mod.set_function("writeText",
      [](const std::string& path,
         const std::string& content) -> bool {
        auto result = fs::writeText(path, content);
        if (result) return true;
        debug::warn(std::format("fs.writeText: {} ({})",
                     result.error().message, path));
        return false;
      });

  fs_mod.set_function("writeBytes",
      [](const std::string& path,
         const std::string& data) -> bool {
        std::vector<uint8_t> vec(data.begin(), data.end());
        auto result = fs::writeBytes(path, vec);
        if (result) return true;
        debug::warn(std::format("fs.writeBytes: {} ({})",
                     result.error().message, path));
        return false;
      });

  fs_mod.set_function("exists", [](const std::string& path) -> bool {
    return fs::exists(path);
  });

  fs_mod.set_function("resolve", [](const std::string& root,
                                      const std::string& relpath) -> std::string {
    return fs::resolve(root, relpath);
  });

  // Convert a single DirEntry to a Lua table
  auto dirEntry_to_table = [runtime](const fs::DirEntry& de) -> sol::table {
    sol::table t = (*runtime->lua_state).create_table();
    t["name"]   = de.name;
    t["path"]   = de.path;
    t["is_dir"] = de.is_dir;
    return t;
  };

  // List directory contents
  fs_mod.set_function("listDir",
      [runtime, dirEntry_to_table](const std::string& path) -> sol::table {
        auto result = fs::listDir(path);
        sol::table entries = (*runtime->lua_state).create_table();
        if (result) {
          for (size_t i = 0; i < result->size(); ++i) {
            entries[i + 1] = dirEntry_to_table((*result)[i]);
          }
        } else {
          debug::warn(std::format("fs.listDir: {} ({})",
                       result.error().message, path));
        }
        return entries;
      });

  coconut["fs"] = fs_mod;

  // ── Store: coconut.store ─────────────────────────────────────
  // Key-value store with event-driven sync to JS.
  {
    sol::table store_mod = (*runtime->lua_state).create_table();

    store_mod.set_function("set",
        [runtime](const std::string& key, const std::string& value) {
          if (!runtime->app || !runtime->app->bridge_state ||
              !runtime->app->bridge_state->store) {
            debug::warn("store.set: store is null");
            return;
          }
          store::set(runtime->app->bridge_state->store, key, value);

          // Emit store:update event to JS (not back to Lua to avoid loops)
          if (runtime->app->bridge_state->transport) {
            nlohmann::json payload = {{"key", key}, {"value", value}};
            bridge::emitToJS(runtime->app, "store:update", payload);
          }
        });

    store_mod.set_function("get",
        [runtime](const std::string& key) -> sol::object {
          if (!runtime->app || !runtime->app->bridge_state ||
              !runtime->app->bridge_state->store) {
            debug::warn("store.get: store is null");
            return sol::lua_nil;
          }
          auto result = store::get(runtime->app->bridge_state->store, key);
          if (result) {
            return sol::make_object(runtime->lua_state->lua_state(), *result);
          } else {
            debug::warn(std::format("store.get: {}", result.error().message));
            return sol::lua_nil;
          }
        });

    store_mod.set_function("has",
        [runtime](const std::string& key) -> bool {
          if (!runtime->app || !runtime->app->bridge_state ||
              !runtime->app->bridge_state->store) {
            debug::warn("store.has: store is null");
            return false;
          }
          return store::has(runtime->app->bridge_state->store, key);
        });

    store_mod.set_function("delete",
        [runtime](const std::string& key) {
          if (!runtime->app || !runtime->app->bridge_state ||
              !runtime->app->bridge_state->store) {
            debug::warn("store.delete: store is null");
            return;
          }
          store::remove(runtime->app->bridge_state->store, key);

          // Emit store:update event to JS
          if (runtime->app->bridge_state->transport) {
            nlohmann::json payload = {{"key", key}, {"value", nullptr}};
            bridge::emitToJS(runtime->app, "store:update", payload);
          }
        });

    store_mod.set_function("clear", [runtime]() {
      if (!runtime->app || !runtime->app->bridge_state ||
          !runtime->app->bridge_state->store) {
        debug::warn("store.clear: store is null");
        return;
      }
      store::clear(runtime->app->bridge_state->store);

      // Emit store:update event to JS
      if (runtime->app->bridge_state->transport) {
        nlohmann::json payload = {{"key", ""}, {"value", nullptr}};
        bridge::emitToJS(runtime->app, "store:update", payload);
      }
    });

    store_mod.set_function("keys", [runtime]() -> sol::table {
      sol::table result = (*runtime->lua_state).create_table();
      if (!runtime->app || !runtime->app->bridge_state ||
          !runtime->app->bridge_state->store) {
        debug::warn("store.keys: store is null");
        return result;
      }
      auto keys_vec = store::keys(runtime->app->bridge_state->store);
      for (size_t i = 0; i < keys_vec.size(); ++i) {
        result[i + 1] = keys_vec[i];
      }
      return result;
    });

    coconut["store"] = store_mod;
  }

  // ── Environment table: coconut.env ──────────────────────────
  // Uses __index metamethod so coconut.env.HOME lazily calls getenv().
  {
    sol::table env_tbl = (*runtime->lua_state).create_table();
    sol::table mt = (*runtime->lua_state).create_table();

    mt["__index"] = [runtime](sol::table, const std::string& key) -> sol::object {
      if (key == "cwd") {
        return sol::make_object(runtime->lua_state->lua_state(),
                                env::cwd());
      }
      if (key == "homedir") {
        return sol::make_object(runtime->lua_state->lua_state(),
                                env::homedir());
      }
      if (key == "pathSeparator") {
        return sol::make_object(runtime->lua_state->lua_state(),
                                std::string(1, env::pathSeparator()));
      }
      std::string val = env::get(key);
      if (val.empty()) {
        return sol::lua_nil;
      }
      return sol::make_object(runtime->lua_state->lua_state(), val);
    };

    // Set the metatable so __index is active for lookups on env_tbl.
    env_tbl[sol::metatable_key] = mt;

    coconut["env"] = env_tbl;
  }

  // ── Open URL ──────────────────────────────────────────────────
  coconut.set_function("openUrl", [](const std::string& url) -> bool {
    return open_url::open(url);
  });

  // ── Clipboard ─────────────────────────────────────────────────
  {
    sol::table cb = (*runtime->lua_state).create_table();
    cb.set_function("readText", []() -> std::string {
      return clipboard::readText();
    });
    cb.set_function("writeText", [](const std::string& text) -> bool {
      return clipboard::writeText(text);
    });
    coconut["clipboard"] = cb;
  }

  // ── Notifications ──────────────────────────────────────────────
  coconut.set_function("notify",
      [](const std::string& title, const std::string& body) -> bool {
        return notify::notify(title, body);
      });

  // ── Keybind system (hybrid chain: top-down JS→Lua, bottom-up Platform→Lua) ─
  coconut["_keybinds"] = runtime->lua_state->create_table();
  coconut.set_function("keybind", [runtime](sol::this_state s,
                                       const std::string& combo,
                                       sol::protected_function handler,
                                       sol::optional<sol::table> opts_tbl) -> sol::function {
    sol::state_view lua(s);

    // Build entry table
    std::string id = opts_tbl ? opts_tbl.value()["id"].get_or(combo) : combo;
    std::string scope = opts_tbl ? opts_tbl.value()["scope"].get_or(std::string("global")) : "global";
    bool platform = opts_tbl ? opts_tbl.value()["platform"].get_or(false) : false;

    // If platform-level, register with App's platform_keybinds set
    if (platform && runtime->app) {
      runtime->app->platform_keybinds.insert(combo);
      debug::info(std::format("[keybind] registered platform keybind: {}", combo));
    }

    // Store in coconut._keybinds[combo] list
    sol::table coconut = lua["coconut"];
    sol::table keybinds = coconut["_keybinds"];
    sol::table list = keybinds[combo];
    if (!list.valid()) {
      list = lua.create_table();
      keybinds[combo] = list;
    }
    list[list.size() + 1] = handler;

    // Also store metadata under a parallel table for lookup
    sol::table meta = coconut["_keybind_meta"];
    if (!meta.valid()) {
      meta = lua.create_table();
      coconut["_keybind_meta"] = meta;
    }
    sol::table meta_entry = lua.create_table();
    meta_entry["id"] = id;
    meta_entry["combo"] = combo;
    meta_entry["scope"] = scope;
    meta_entry["platform"] = platform;
    meta[id] = meta_entry;

    // Return unregister function (simple C++ callable)
    sol::function unreg_fn = lua["__coconut_unregister_keybind"];
    if (!unreg_fn.valid()) {
      // Create one-time helper in Lua
      lua.script(R"(
        __coconut_unregister_keybind = function(combo, id)
          local c = coconut
          if c._keybinds and c._keybinds[combo] then
            c._keybinds[combo] = nil
          end
          if c._keybind_meta and c._keybind_meta[id] then
            c._keybind_meta[id] = nil
          end
        end
      )");
    }
    return lua.script("return function() end");
  });

  // Register Lua-side cleanup helper
  {
    sol::state_view lv(*runtime->lua_state);
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

  // ── Event dispatch system ──────────────────────────────────────────
  // Injects the Lua-side event object model, subscribe API, and central
  // dispatcher.  This replaces the old (name, payload, ctx) triple with
  // a DOM-like event object and three-tier dispatch chain.
  {
    auto result = runtime->lua_state->safe_script(R"(
      coconut._listeners = {}

      -- Event factory: builds a DOM-like event object with methods.
      function coconut._makeEvent(name, payload, target)
        local methods = {
          preventDefault = function(self)
            self.defaultPrevented = true
          end,
          stopPropagation = function(self)
            self.propagationStopped = true
          end,
          stopImmediatePropagation = function(self)
            self.defaultPrevented = true
            self.propagationStopped = true
          end,
        }
        local event = setmetatable({
          name = name,
          target = target or "",
          defaultPrevented = false,
          propagationStopped = false,
        }, {
          __index = function(_, key)
            if key == "type" then return name end
            return methods[key]
          end
        })
        -- Merge payload fields (skip name/type)
        if type(payload) == "table" then
          for k, v in pairs(payload) do
            if k ~= "name" and k ~= "type" then
              event[k] = v
            end
          end
        end
        return event
      end

      -- Subscribe API: coconut.on(name, fn, { once? }) -> unregister fn
      function coconut.on(name, fn, opts)
        opts = opts or {}
        if not coconut._listeners[name] then
          coconut._listeners[name] = {}
        end
        local entry = { fn = fn, once = opts.once == true }
        table.insert(coconut._listeners[name], entry)
        return function()
          for i, e in ipairs(coconut._listeners[name]) do
            if e == entry then
              table.remove(coconut._listeners[name], i)
              return
            end
          end
        end
      end

      -- Central dispatcher: routes through three-tier chain.
      -- Called by both C++ bridge (dispatchEventToLua) and ctx:emit().
      function coconut._dispatch(name, payload, target)
        local event = coconut._makeEvent(name, payload, target or "")

        -- Tier 1: active view's on(name, fn)
        local active = coconut._active_view
        if active and active ~= "" then
          local registry = coconut._view_descriptors
          if registry and registry[active] then
            local view = registry[active]
            if view._callbacks and view._callbacks[name] then
              view._callbacks[name](event)
              if event.propagationStopped then return event end
            end
          end
        end

        -- Tier 2: global coconut.on(name, fn) — FIFO
        -- Snapshot listeners before iteration so self-unsubscribe
        -- during dispatch doesn't corrupt the loop.
        local listeners = coconut._listeners[name]
        if listeners then
          local snapshot = {}
          for i = 1, #listeners do
            snapshot[i] = listeners[i]
          end
          for _, entry in ipairs(snapshot) do
            entry.fn(event)
            if entry.once then
              for i = #listeners, 1, -1 do
                if listeners[i] == entry then
                  table.remove(listeners, i)
                  break
                end
              end
            end
            if event.propagationStopped then return event end
          end
        end

        -- Tier 3: coconut.events(event) fallback
        if type(coconut.events) == "function" then
          coconut.events(event)
        end

        return event
      end

      -- coconut.emit(event) — send event to JS via bridge.
      -- Overrides the C++ binding with a Lua function that also
      -- runs the three-tier dispatch chain on the Lua side.
      coconut.emit = function(event)
        if type(event) ~= "table" then
          error("coconut.emit expects a table with a 'name' field")
        end
        local name = event.name
        if not name then
          error("coconut.emit: event must have a 'name' field")
        end

        -- Build a clean payload for JS (no metatable methods)
        local payload = {}
        for k, v in pairs(event) do
          if k ~= "name" and k ~= "type" and k ~= "target"
             and k ~= "preventDefault" and k ~= "stopPropagation"
             and k ~= "stopImmediatePropagation"
             and k ~= "defaultPrevented" and k ~= "propagationStopped" then
            payload[k] = v
          end
        end

        local target = coconut._active_view or ""

        -- Run Lua dispatch chain
        coconut._dispatch(name, payload, target)

        -- Forward to JS via low-level bridge helper
        coconut._bridge_emit(name, coconut.json.jsonify(payload))
      end
    )", sol::script_pass_on_error);
    if (!result.valid()) {
      sol::error err = result;
      debug::warn(std::format("event dispatch system: {}", err.what()));
    }
  }

  runtime->lua_state->set("coconut", coconut);
}

void _bindViewClass(Runtime *runtime) {
  // Build the View module as Lua code so the descriptor methods (defineProps,
  // on_load, on_mount, on_unmount, on) are easy to read and
  // maintain.  Each factory creates a table with kind/value and chainable
  // lifecycle methods.
  const char* viewSrc = R"(
    local function makeDescriptor(kind, value)
      return {
        kind = kind,
        value = value,
        _props = {},
        _callbacks = {},

        defineProps = function(self, props)
          self._props = props or {}
          return self
        end,

        -- Lifecycle hooks are sugar over the generic view:on(name, fn).
        -- They store under unified keys ("load", "mount", "unmount", "close")
        -- so Tier 1 of _dispatch can find them by event name.
        on_load = function(self, fn)
          self:on("load", fn)
          return self
        end,

        on_mount = function(self, fn)
          self:on("mount", fn)
          return self
        end,

        on_unmount = function(self, fn)
          self:on("unmount", fn)
          return self
        end,

        on_before_close = function(self, fn)
          self:on("close", fn)
          return self
        end,

        on = function(self, name, fn)
          self._callbacks[name] = fn
          return self
        end,
      }
    end

    View = {
      url  = function(url)  return makeDescriptor('url',  url) end,
      html = function(html) return makeDescriptor('html', html) end,
      load = function(path) return makeDescriptor('file', path) end,
    }
  )";

  runtime->lua_state->script(viewSrc);
  runtime->lua_state->set("View", (*runtime->lua_state)["View"]);
}

void _bindUserType(Runtime *runtime) {
  // setWindowSize receives a Lua table { w = ..., h = ... }.
  // We bind it as a lambda to avoid requiring sol2 to auto-convert the
  // plain CoconutWindowSize struct (which sol2 v3.3 doesn't support
  // for member-pointer-based usertype registration).
  auto setWindowSize = [](CoconutContext* ctx, sol::table t) -> CoconutContext* {
    if (ctx != nullptr && ctx->configs != nullptr) {
      ctx->configs->window_width = t["w"].get_or(1280);
      ctx->configs->window_height = t["h"].get_or(640);
    }
    return ctx;
  };

  // Minimum/maximum window size setters also take { w = ..., h = ... }.
  auto setMinimumWindowSize = [](CoconutContext* ctx, sol::table t) -> CoconutContext* {
    if (ctx != nullptr && ctx->configs != nullptr) {
      ctx->configs->window_min_width = t["w"].get_or(0);
      ctx->configs->window_min_height = t["h"].get_or(0);
    }
    return ctx;
  };

  auto setMaximumWindowSize = [](CoconutContext* ctx, sol::table t) -> CoconutContext* {
    if (ctx != nullptr && ctx->configs != nullptr) {
      ctx->configs->window_max_width = t["w"].get_or(0);
      ctx->configs->window_max_height = t["h"].get_or(0);
    }
    return ctx;
  };

  // Window handle getter — sol::readonly would not work here because
  // the handle pointer may be null during registration.  A sol::property
  // getter/setter pair lets C++ control assignment while Lua can read
  // the current value (even as it changes post-registration).
  runtime->lua_state->new_usertype<CoconutContext>(
      "CoconutContext",
      "window", sol::property(
          [](CoconutContext* ctx) -> CoconutWindowHandle* {
            return ctx ? ctx->window_handle : nullptr;
          },
          [](CoconutContext* ctx, CoconutWindowHandle* h) {
            if (ctx) ctx->window_handle = h;
          }),
      "setWindowSize", std::move(setWindowSize),
      "setMinimumWindowSize", std::move(setMinimumWindowSize),
      "setMaximumWindowSize", std::move(setMaximumWindowSize),
      "setMinimumWindowWidth", &CoconutContext::setMinimumWindowWidth,
      "setMinimumWindowHeight", &CoconutContext::setMinimumWindowHeight,
      "setMaximumWindowWidth", &CoconutContext::setMaximumWindowWidth,
      "setMaximumWindowHeight", &CoconutContext::setMaximumWindowHeight,
      "setTitle", &CoconutContext::setTitle,
      "setResizable", &CoconutContext::setResizable,
      "setFrameless", &CoconutContext::setFrameless,
      "setTransparent", &CoconutContext::setTransparent,
      "setInitialView", &CoconutContext::setInitialView,
      "show",   &CoconutContext::show,
      "reload", &CoconutContext::reload,
      "close",  &CoconutContext::close,
      "bind",   &CoconutContext::bind,
      "emit",    &CoconutContext::emit,
      "emit_sync", &CoconutContext::emit_sync);

  // ── CoconutWindowHandle usertype ───────────────────────────────
  // move takes a table { x = dx, y = dy } — sol3 can't auto-convert
  // CoconutPoint from a Lua table, so we use a lambda.
  auto moveHandle = [](CoconutWindowHandle* h, sol::table t) {
    if (!h) return;
    CoconutPoint pt{};
    pt.x = t["x"].get_or(0);
    pt.y = t["y"].get_or(0);
    h->move(pt);
  };

  runtime->lua_state->new_usertype<CoconutWindowHandle>(
      "CoconutWindow",
      "show",           &CoconutWindowHandle::show,
      "reload",         &CoconutWindowHandle::reload,
      "close",          &CoconutWindowHandle::close,
      "minimize",       &CoconutWindowHandle::minimize,
      "maximize",       &CoconutWindowHandle::maximize,
      "setFullscreen",  &CoconutWindowHandle::setFullscreen,
      "toggleFullscreen", &CoconutWindowHandle::toggleFullscreen,
      "resize",         &CoconutWindowHandle::resize,
      "setMovableByBackground", &CoconutWindowHandle::setMovableByBackground,
      "setBackgroundColor", &CoconutWindowHandle::setBackgroundColor,
      "setPosition",    &CoconutWindowHandle::setPosition,
      "getPosition",    &CoconutWindowHandle::getPosition,
      "move",           std::move(moveHandle));

  // ctx.window is set later (after app pointer is wired) via
  // lua::wireWindowHandle(runtime).

  // Always set the base ctx global so Lua can access CoconutContext methods.
  runtime->lua_state->set("ctx", runtime->context);
}

void destroy(Runtime *runtime) {
  if (runtime == nullptr) {
    return;
  }

  delete runtime->lua_state;
  delete runtime;
}

// ── Command invocation ──────────────────────────────────────────────────

std::expected<sol::object, Error> call(Runtime* runtime,
                                       const std::string& name,
                                       sol::table params) {
  if (runtime == nullptr || runtime->lua_state == nullptr) {
    return std::unexpected(Error{
        .code = ErrorCode::InvalidConfig,
        .message = "call: null runtime or lua_state",
    });
  }
  if (runtime->app == nullptr || runtime->app->commands == nullptr) {
    return std::unexpected(Error{
        .code = ErrorCode::InvalidConfig,
        .message = "call: app or commands registry not wired",
    });
  }

  auto& handlers = runtime->app->commands->handlers;
  auto it = handlers.find(name);
  if (it == handlers.end()) {
    return std::unexpected(Error{
        .code = ErrorCode::CommandNotFound,
        .message = "command not registered: " + name,
    });
  }

  // Handler signature: fn(params, ctx)
  auto result = it->second(params, runtime->context);
  if (!result.valid()) {
    sol::error err = result;
    return std::unexpected(Error{
        .code = ErrorCode::LuaError,
        .message = "command '" + name + "' failed",
        .details = err.what(),
    });
  }

  return result;
}

// ── View lifecycle dispatch ───────────────────────────────────────────

void dispatchViewLifecycleEvent(Runtime* runtime,
                                  const std::string& viewName,
                                  const std::string& eventName,
                                  sol::table extraPayload) {
  if (runtime == nullptr || runtime->lua_state == nullptr ||
      runtime->context == nullptr) {
    return;
  }

  sol::state& lua = *runtime->lua_state;

  // Look up the view descriptor registry.
  sol::object registry = lua["coconut"]["_view_descriptors"];
  if (!registry.is<sol::table>()) {
    return;
  }

  sol::object desc = registry.as<sol::table>()[viewName];
  if (!desc.is<sol::table>()) {
    // View not registered in descriptor registry — normal for views
    // created from config file rather than coconut.views().
    return;
  }

  sol::table descriptor = desc.as<sol::table>();

  // Guard: "load" fires only once per view.
  if (eventName == "load") {
    sol::object loaded = descriptor["_loaded"];
    if (loaded.is<bool>() && loaded.as<bool>()) {
      return;
    }
    descriptor["_loaded"] = true;
  }

  // Build payload with ctx and view props so handlers can access
  // them via event.ctx / event.props (same shape for all lifecycle events).
  sol::table payload = lua.create_table();
  payload["ctx"] = runtime->context;
  sol::table props = descriptor["_props"];
  if (props.is<sol::table>()) {
    payload["props"] = props;
  } else {
    payload["props"] = lua.create_table();
  }

  // Merge extra payload fields (e.g. {w=1024, h=768} for resize).
  // Guard: extraPayload may be a default-constructed table (null lua_State*).
  if (extraPayload.valid() && extraPayload.lua_state() != nullptr) {
    for (const auto& kv : extraPayload) {
      payload[kv.first] = kv.second;
    }
  }

  // Dispatch through the three-tier event system.
  sol::function dispatch = lua["coconut"]["_dispatch"];
  if (!dispatch.valid()) {
    return;
  }

  auto result = dispatch(eventName, payload, viewName);
  if (!result.valid()) {
    sol::error err = result;
    debug::warn(std::format("lifecycle event '{}' for '{}' failed: {}",
                            eventName, viewName, err.what()));
  }
}

// ── Entry-point loader ──────────────────────────────────────────────────

std::expected<bool, Error> loadEntryPoint(Runtime* runtime, Config* cfg) {
  if (runtime == nullptr || runtime->lua_state == nullptr) {
    return std::unexpected(Error{
        .code = ErrorCode::InvalidConfig,
        .message = "loadEntryPoint: null runtime or lua_state",
    });
  }

  // Probe for main.lua first — missing file is not an error,
  // it just means the app runs on config-file defaults alone.
  {
    std::ifstream probe("main.lua");
    if (!probe.is_open()) {
      debug::info("no main.lua, skipping entry-point config");
      return false;
    }
  }

  sol::state& lua = *runtime->lua_state;

  // ── Load main.lua ──────────────────────────────────────────────────
  auto entry = lua.safe_script_file("main.lua", sol::script_pass_on_error);
  if (!entry.valid()) {
    sol::error err = entry;
    return std::unexpected(Error{
        .code = ErrorCode::LuaError,
        .message = "failed to load main.lua",
        .details = err.what(),
    });
  }

  debug::info("loaded main.lua");

  // ── coconut.config(ctx) ────────────────────────────────────────────
  sol::object config_fn = lua["coconut"]["config"];
  if (config_fn.is<sol::function>()) {
    debug::info("calling coconut.config(ctx)...");
    // ctx is set as a global by _bindUserType — grab it.
    sol::object ctx_obj = lua["ctx"];

    auto result = config_fn.as<sol::function>()(ctx_obj);
    if (!result.valid()) {
      sol::error err = result;
      return std::unexpected(Error{
          .code = ErrorCode::LuaError,
          .message = "coconut.config(ctx) failed",
          .details = err.what(),
      });
    }

    // If the callback returned a table, merge additional fields.
    // The ctx setters already mutated the Config in-place for
    // setBrowser / setWindowSize / setInitialView calls.
    sol::object ret = result;
    if (ret.is<sol::table>() && cfg != nullptr) {
      sol::table t = ret.as<sol::table>();

      auto mergeStr = [&](const char* key, std::string& field) {
        sol::object v = t[key];
        if (v.is<std::string>()) field = v.as<std::string>();
      };
      auto mergeInt = [&](const char* key, int& field) {
        sol::object v = t[key];
        if (v.is<int>()) field = v.as<int>();
      };

      mergeInt("window_width",    cfg->window_width);
      mergeInt("window_height",   cfg->window_height);
      mergeInt("window_min_width",  cfg->window_min_width);
      mergeInt("window_min_height", cfg->window_min_height);
      mergeInt("window_max_width",  cfg->window_max_width);
      mergeInt("window_max_height", cfg->window_max_height);
      mergeStr("initial_view",    cfg->initial_view);
      mergeStr("title",            cfg->title);
      mergeStr("view_root",       cfg->view_root);
      mergeStr("asset_root",      cfg->asset_root);
      mergeStr("command_root",    cfg->command_root);

      // Merge views block from returned table.
      sol::object views = t["views"];
      if (views.is<sol::table>()) {
        for (auto& [k, v] : views.as<sol::table>()) {
          if (!v.is<sol::table>()) continue;
          sol::table vt = v.as<sol::table>();
          std::string name = k.as<std::string>();
          std::string kind = vt["kind"].get_or<std::string>("");
          if (kind != "file" && kind != "html" && kind != "url") continue;
          cfg->views[name] = ViewEntry{
              .kind = std::move(kind),
              .src = vt["src"].get_or<std::string>(""),
          };
        }
      }
    }

    debug::info("coconut.config(ctx) applied");
  }

  // ── coconut.views() ────────────────────────────────────────────────
  // App-level view definitions complement those from the config file.
  // Views with the same name overwrite config-file entries.
  debug::info("checking coconut.views()...");
  sol::object views_fn = lua["coconut"]["views"];
  if (views_fn.is<sol::function>()) {
    debug::info("calling coconut.views()...");
    auto views_result = views_fn.as<sol::function>()();
    if (views_result.valid()) {
      sol::object views_obj = views_result;
      if (views_obj.is<sol::table>()) {
        debug::info("coconut.views() returned view descriptors");
        sol::table vt = views_obj.as<sol::table>();
        sol::table registry = lua.create_table();
        for (auto& [k, v] : vt) {
          if (!v.is<sol::table>()) continue;
          sol::table desc = v.as<sol::table>();
          std::string name = k.as<std::string>();
          std::string kind = desc["kind"].get_or<std::string>("");
          std::string value = desc["value"].get_or<std::string>("");
          if (kind.empty()) continue;
          cfg->views[name] = ViewEntry{.kind = std::move(kind),
                                        .src = std::move(value)};
          registry[name] = desc;
          debug::info(std::format("view '{}' ({})", name, cfg->views[name].kind));
        }
        lua["coconut"]["_view_descriptors"] = registry;
        debug::info("stored view descriptors in coconut._view_descriptors");
      }
    }
  }

  // ── coconut.commands(ctx) [manual override] ──────────────────────
  // If the user's main.lua defines coconut.commands(), call it first.
  // This gives explicit control before the auto-loader runs.
  debug::info("checking coconut.commands()...");
  sol::object cmds_fn = lua["coconut"]["commands"];
  if (cmds_fn.is<sol::function>()) {
    debug::info("calling coconut.commands(ctx)...");
    sol::object ctx_obj = lua["ctx"];
    auto cmds_result = cmds_fn.as<sol::function>()(ctx_obj);
    if (!cmds_result.valid()) {
      sol::error err = cmds_result;
      debug::warn(std::format("coconut.commands(ctx) failed: {}", err.what()));
    } else {
      debug::info("coconut.commands(ctx) applied");
    }
  }

  // ── Auto-load generated commands ──────────────────────────────────
  // Scan the command root directory and the generated directory for
  // .g.lua files.  Each .g.lua exports a register(ctx) function that
  // calls ctx:bind() for each command defined in the corresponding .lua
  // module.
  {
    std::string cmdRoot = cfg ? cfg->command_root : "commands";
    std::string genDir  = "generated";
    debug::info(std::format("scanning {}/ and {}/ for .g.lua files...",
                            cmdRoot, genDir));

    // Add directories to package.path so the .g.lua's require() works.
    std::string pkgPath = ";"
        + cmdRoot + "/?.lua;"
        + cmdRoot + "/?/init.lua;"
        + genDir  + "/?.lua;"
        + genDir  + "/?/init.lua";
    lua.script("package.path = package.path .. '" + pkgPath + "'");

    sol::object ctx_obj = lua["ctx"];
    if (!ctx_obj.valid()) {
      debug::warn("ctx not available, skipping command auto-load");
    } else {
      int loaded = 0;
      std::vector<std::string> dirsToScan = {cmdRoot, genDir};
      try {
        for (const auto& scanDir : dirsToScan) {
        if (!std::filesystem::is_directory(scanDir)) continue;

        for (auto& entry : std::filesystem::directory_iterator(scanDir)) {
          auto path = entry.path();
          if (path.extension() != ".lua") continue;
          auto stem = path.stem().string();

          // Only load .g.lua files (generated command registration wrappers).
          if (stem.size() < 2 ||
              stem.substr(stem.size() - 2) != ".g")
            continue;

          std::string cmdName =
              stem.substr(0, stem.size() - 2);
          debug::info(std::format("found {}.g.lua, loading...", cmdName));

          // Load the .g.lua file — it returns a register function.
          auto loadResult = lua.script_file(path.string(),
              sol::script_pass_on_error);
          if (!loadResult.valid()) {
            sol::error e = loadResult;
            debug::warn(std::format("failed to load {}: {}",
                                    path.filename().string(), e.what()));
            continue;
          }

          // The returned value should be the register function.
          sol::object ret = loadResult;
          if (!ret.is<sol::function>()) {
            debug::warn(std::format("{} did not return a function (returned type {})",
                                    path.filename().string(),
                                    static_cast<int>(ret.get_type())));
            continue;
          }

          // Call register(ctx).
          auto bindResult =
              ret.as<sol::function>()(ctx_obj);
          if (!bindResult.valid()) {
            sol::error e = bindResult;
            debug::warn(std::format("register({}) failed: {}", cmdName, e.what()));
          } else {
            ++loaded;
            debug::info(std::format("registered {} commands", cmdName));
          }
        }
      }
      } catch (const std::filesystem::filesystem_error& err) {
        debug::info(std::format("no {}/ or {}/ directory", cmdRoot, genDir));
      }
      if (loaded > 0) {
        debug::info(std::format("loaded {} command module(s)", loaded));
      }
    }
  }

  debug::info("loadEntryPoint done");
  return true;
}

void wireWindowHandle(Runtime* runtime) {
  if (runtime == nullptr || runtime->lua_state == nullptr ||
      runtime->context == nullptr || runtime->context->window_handle == nullptr) {
    return;
  }
  // Wire the app pointer so window operations can access the webview.
  runtime->context->window_handle->app = runtime->app;

  // Expose the window handle as a Lua global so commands registered via
  // _registerBuiltinCommands (which use a Lua script) can access it
  // directly without going through ctx.window (property getter).
  runtime->lua_state->set("_coconut_window", runtime->context->window_handle);

  debug::info("wired ctx.window to CoconutWindowHandle");
}

} // namespace coconut::lua
