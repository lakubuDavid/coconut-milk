#include "main_runtime.h"
#include "modules/bridge_emit.h"
#include "modules/clipboard.h"
#include "modules/dialog.h"
#include "modules/env.h"
#include "modules/fs.h"
#include "modules/hotreload.h"
#include "modules/json.h"
#include "modules/keybind.h"
#include "modules/log.h"
#include "modules/notify.h"
#include "modules/openurl.h"
#include "modules/store.h"
#include "modules/stubs.h"
#include "modules/thread_kind.h"
#include "modules/window.h"

#include "app.h"
#include "debug.h"
#include "packages/clipboard.h"
#include "packages/env.h"
#include "packages/notify.h"
#include "packages/open_url.h"

#include <sol/state.hpp>
#include <sol/table.hpp>

#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>

// Some system headers (ObjC runtime transitives) define `nil` as a macro
// which clashes with sol::type::nil and other uses. Undefine it here.
#ifdef nil
#undef nil
#endif

namespace coconut::lua {

  std::expected<Runtime*, Error> create(Config* cfg, CoconutContext* ctx) {
    auto runtime = new Runtime{.configs = cfg, .context = ctx, .lua_state = nullptr};

    runtime->lua_state = new sol::state();
    runtime->lua_state->open_libraries(
        sol::lib::jit,
        sol::lib::base,
        sol::lib::package,
        sol::lib::io,
        sol::lib::os,
        sol::lib::table,
        sol::lib::string,
        sol::lib::math
    );

    _bindCoconutLuaApi(runtime);
    _bindViewClass(runtime);
    _bindUserType(runtime);
    _registerBuiltinCommands(runtime);

    // Transport is created by main.cpp after runtime->app is wired.

    return runtime;
  }

  void _registerBuiltinCommands(Runtime* runtime) {
    // Register framework-level commands so the JS bridge can dispatch to them.
    // These wrap the C++ functions bound on the coconut table.
    sol::state& lua = *runtime->lua_state;
    const char* src = R"(
    local ctx = _G.ctx
    if not ctx then return end

    -- Builtin commands run on the MAIN thread because they interact with
    -- platform APIs (clipboard, fs, dialogs, webview, store) that are
    -- not thread-safe or require the main thread.

    ctx:bind_mt("clipboard_read", function()
      return coconut.clipboard.readText()
    end)
    ctx:bind_mt("clipboard_write", function(params)
      return coconut.clipboard.writeText(params.text or "")
    end)
    ctx:bind_mt("openUrl", function(params)
      return coconut.openUrl(params.url or "")
    end)
    ctx:bind_mt("notify", function(params)
      return coconut.notify(params.title or "", params.body or "")
    end)
    ctx:bind_mt("dialog_message", function(params)
      return coconut.dialog.message(params.message or "",
                                     params.title or "Message",
                                     params.kind or "info")
    end)
    ctx:bind_mt("dialog_open", function(params)
      return coconut.dialog.open(params.title or "Open",
                                  params.multi,
                                  params.chooseDir)
    end)
    ctx:bind_mt("dialog_save", function(params)
      return coconut.dialog.save(params.title or "Save",
                                  params.defaultName or "")
    end)
    ctx:bind_mt("fs_exists", function(params)
      local ok, exists = pcall(coconut.fs.exists, params.path)
      if ok then return { ok = true, exists = exists } end
      return { ok = false, error = tostring(exists) }
    end)
    ctx:bind_mt("fs_write_text", function(params)
      local ok, err = pcall(coconut.fs.writeText, params.path, params.content)
      if ok then return { ok = err } end
      return { ok = false, error = tostring(err) }
    end)
    ctx:bind_mt("fs_resolve", function(params)
      local ok, resolved = pcall(coconut.fs.resolve, params.root, params.relpath)
      if ok then return { ok = true, data = resolved } end
      return { ok = false, error = tostring(resolved) }
    end)
    ctx:bind_mt("fs_list_dir", function(params)
      local ok, entries = pcall(coconut.fs.listDir, params.path)
      if ok then return { ok = true, data = entries } end
      return { ok = false, error = tostring(entries) }
    end)
    ctx:bind_mt("ping", function()
      return "pong"
    end)
    ctx:bind_mt("getViews", function()
      local names, i = {}, 1
      for name in pairs(coconut.views()) do
        names[i] = name; i = i + 1
      end
      return names
    end)
    ctx:bind_mt("fs_read_text", function(params)
      local ok, data = pcall(coconut.fs.readText, params.path)
      if ok then return { ok = true, data = data } end
      return { ok = false, error = tostring(data) }
    end)
    ctx:bind_mt("__coconutWindowCtl", function(params)
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
    ctx:bind_mt("__registerPlatformKeybind", function(params)
      local combo = params.combo
      if combo and coconut.__registerPlatformKeybind then
        local ok = pcall(coconut.__registerPlatformKeybind, params)
        return { ok = ok }
      end
      return { ok = false, error = "missing combo or binding" }
    end)

    -- ── Window settings (Settings view) ──
    -- Thin wrappers over the coconut.window module: on the main thread
    -- they run inline; from a worker the same calls marshal onto the main
    -- run loop via dispatch::post. Single API, correct dispatch either way.
    ctx:bind_mt("set_window_title", function(params)
      return coconut.window.setTitle(tostring(params.title or ""))
    end)
    ctx:bind_mt("set_window_resizable", function(params)
      return coconut.window.setResizable(params.resizable == true)
    end)
    ctx:bind_mt("set_window_min_size", function(params)
      return coconut.window.setMinimumSize(tonumber(params.w) or 0, tonumber(params.h) or 0)
    end)
    ctx:bind_mt("set_window_max_size", function(params)
      return coconut.window.setMaximumSize(tonumber(params.w) or 0, tonumber(params.h) or 0)
    end)
    ctx:bind_mt("set_window_size", function(params)
      return coconut.window.setSize(tonumber(params.w) or 800, tonumber(params.h) or 600)
    end)
    ctx:bind_mt("set_window_min_width", function(params)
      local cur = ctx.configs and ctx.configs.window_min_height or 0
      return coconut.window.setMinimumSize(tonumber(params.width) or 0, cur)
    end)
    ctx:bind_mt("set_window_min_height", function(params)
      local cur = ctx.configs and ctx.configs.window_min_width or 0
      return coconut.window.setMinimumSize(cur, tonumber(params.height) or 0)
    end)
    ctx:bind_mt("set_window_max_width", function(params)
      local cur = ctx.configs and ctx.configs.window_max_height or 0
      return coconut.window.setMaximumSize(tonumber(params.width) or 0, cur)
    end)
    ctx:bind_mt("set_window_max_height", function(params)
      local cur = ctx.configs and ctx.configs.window_max_width or 0
      return coconut.window.setMaximumSize(cur, tonumber(params.height) or 0)
    end)
    ctx:bind_mt("set_window_position", function(params)
      return coconut.window.setPosition(tonumber(params.x) or 0, tonumber(params.y) or 0)
    end)
    ctx:bind_mt("get_window_position", function()
      return coconut.window.getPosition()
    end)
    ctx:bind_mt("get_window_position", function()
      return coconut.window.getPosition()
    end)
    ctx:bind_mt("store_set", function(params)
      local ok, err = pcall(coconut.store.set, params.key, params.value)
      if ok then return { ok = true } end
      return { ok = false, error = tostring(err) }
    end)
    ctx:bind_mt("store_get", function(params)
      local ok, value = pcall(coconut.store.get, params.key)
      if ok then return { ok = true, value = value } end
      return { ok = false, error = tostring(value) }
    end)
    ctx:bind_mt("store_has", function(params)
      local ok, has = pcall(coconut.store.has, params.key)
      if ok then return { ok = true, has = has } end
      return { ok = false, error = tostring(has) }
    end)
    ctx:bind_mt("store_delete", function(params)
      local ok, err = pcall(coconut.store.delete, params.key)
      if ok then return { ok = true } end
      return { ok = false, error = tostring(err) }
    end)
    ctx:bind_mt("store_clear", function()
      local ok, err = pcall(coconut.store.clear)
      if ok then return { ok = true } end
      return { ok = false, error = tostring(err) }
    end)
    ctx:bind_mt("store_keys", function()
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

  void _bindCoconutLuaApi(Runtime* runtime) {
    sol::table coconut = (*runtime->lua_state)["coconut"].get_or_create<sol::table>();
    auto&      lua     = *runtime->lua_state;

    // Store runtime reference for modules that need App* access.
    // Modules look up lua["coconut"]["_app"] for store, hotreload, etc.
    coconut["_runtime"] = runtime;
    if (runtime->app) {
      coconut["_app"] = runtime->app;
    }

    // ── Module registration (call each module with ThreadKind::Main) ───
    using namespace coconut::modules;
    init_log(lua, ThreadKind::Main);
    init_json(lua, ThreadKind::Main);
    init_fs(lua, ThreadKind::Main);
    init_dialog(lua, ThreadKind::Main);
    init_notify(lua, ThreadKind::Main);
    init_clipboard(lua, ThreadKind::Main);
    init_store(lua, ThreadKind::Main);
    init_window(lua, ThreadKind::Main);
    init_env(lua, ThreadKind::Main);
    init_openurl(lua, ThreadKind::Main);
    init_hotreload(lua, ThreadKind::Main);
    init_bridge_emit(lua, ThreadKind::Main);
    init_keybind(lua, ThreadKind::Main);
    init_stubs(lua, ThreadKind::Main);

    // ── Event dispatch system ──────────────────────────────────────────
    // Injects the Lua-side event object model, subscribe API, and central
    // dispatcher.  This replaces the old (name, payload, ctx) triple with
    // a DOM-like event object and three-tier dispatch chain.
    {
      auto result = runtime->lua_state->safe_script(
          R"(
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
    )",
          sol::script_pass_on_error
      );
      if (!result.valid()) {
        sol::error err = result;
        debug::warn(std::format("event dispatch system: {}", err.what()));
      }
    }

    runtime->lua_state->set("coconut", coconut);
  }

  void _bindViewClass(Runtime* runtime) {
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

  void _bindUserType(Runtime* runtime) {
    // setWindowSize receives a Lua table { w = ..., h = ... }.
    // We bind it as a lambda to avoid requiring sol2 to auto-convert the
    // plain CoconutWindowSize struct (which sol2 v3.3 doesn't support
    // for member-pointer-based usertype registration).
    auto setWindowSize = [](CoconutContext* ctx, sol::table t) -> CoconutContext* {
      if (ctx != nullptr && ctx->configs != nullptr) {
        ctx->configs->window_width  = t["w"].get_or(1280);
        ctx->configs->window_height = t["h"].get_or(640);
      }
      return ctx;
    };

    // Minimum/maximum window size setters also take { w = ..., h = ... }.
    auto setMinimumWindowSize = [](CoconutContext* ctx, sol::table t) -> CoconutContext* {
      if (ctx != nullptr && ctx->configs != nullptr) {
        ctx->configs->window_min_width  = t["w"].get_or(0);
        ctx->configs->window_min_height = t["h"].get_or(0);
      }
      return ctx;
    };

    auto setMaximumWindowSize = [](CoconutContext* ctx, sol::table t) -> CoconutContext* {
      if (ctx != nullptr && ctx->configs != nullptr) {
        ctx->configs->window_max_width  = t["w"].get_or(0);
        ctx->configs->window_max_height = t["h"].get_or(0);
      }
      return ctx;
    };

    // Window handle getter — sol::readonly would not work here because
    // the handle pointer may be null during registration.  A sol::property
    // getter/setter pair lets C++ control assignment while Lua can read
    // the current value (even as it changes post-registration).
    // CoconutContext usertype — single source of truth in context.cpp.
    context::registerUsertype(*runtime->lua_state);

    // ── CoconutWindowHandle usertype ───────────────────────────────
    // move takes a table { x = dx, y = dy } — sol3 can't auto-convert
    // CoconutPoint from a Lua table, so we use a lambda.
    auto moveHandle = [](CoconutWindowHandle* h, sol::table t) {
      if (!h)
        return;
      CoconutPoint pt{};
      pt.x = t["x"].get_or(0);
      pt.y = t["y"].get_or(0);
      h->move(pt);
    };

    runtime->lua_state->new_usertype<CoconutWindowHandle>(
        "CoconutWindow",
        "show",
        &CoconutWindowHandle::show,
        "reload",
        &CoconutWindowHandle::reload,
        "close",
        &CoconutWindowHandle::close,
        "minimize",
        &CoconutWindowHandle::minimize,
        "maximize",
        &CoconutWindowHandle::maximize,
        "setFullscreen",
        &CoconutWindowHandle::setFullscreen,
        "toggleFullscreen",
        &CoconutWindowHandle::toggleFullscreen,
        "resize",
        &CoconutWindowHandle::resize,
        "setMovableByBackground",
        &CoconutWindowHandle::setMovableByBackground,
        "setBackgroundColor",
        &CoconutWindowHandle::setBackgroundColor,
        "setPosition",
        &CoconutWindowHandle::setPosition,
        "getPosition",
        [](CoconutWindowHandle* h, sol::this_state s) -> sol::table {
          // Wrap CoconutPoint as {x, y} table — raw struct pushes as
          // opaque userdata with unreadable fields.
          sol::state_view lv(s);
          sol::table      t = lv.create_table();
          if (h != nullptr) {
            CoconutPoint p = h->getPosition();
            t["x"]         = p.x;
            t["y"]         = p.y;
          } else {
            t["x"] = 0;
            t["y"] = 0;
          }
          return t;
        },
        "setTitle",
        &CoconutWindowHandle::setTitle,
        "setResizable",
        &CoconutWindowHandle::setResizable,
        "setMinimumSize",
        &CoconutWindowHandle::setMinimumSize,
        "setMaximumSize",
        &CoconutWindowHandle::setMaximumSize,
        "move",
        std::move(moveHandle)
    );

    // ctx.window is set later (after app pointer is wired) via
    // lua::wireWindowHandle(runtime).

    // Always set the base ctx global so Lua can access CoconutContext methods.
    runtime->lua_state->set("ctx", runtime->context);
  }

  void destroy(Runtime* runtime) {
    if (runtime == nullptr) {
      return;
    }

    delete runtime->lua_state;
    delete runtime;
  }

  // ── Command invocation ──────────────────────────────────────────────────

  std::expected<sol::object, Error> call(
      Runtime* runtime, const std::string& name, sol::table params
  ) {
    if (runtime == nullptr || runtime->lua_state == nullptr) {
      return std::unexpected(Error{
          .code    = ErrorCode::InvalidConfig,
          .message = "call: null runtime or lua_state",
      });
    }
    if (runtime->app == nullptr || runtime->app->commands == nullptr) {
      return std::unexpected(Error{
          .code    = ErrorCode::InvalidConfig,
          .message = "call: app or commands registry not wired",
      });
    }

    auto& handlers = runtime->app->commands->handlers;
    auto  it       = handlers.find(name);
    if (it == handlers.end()) {
      return std::unexpected(Error{
          .code    = ErrorCode::CommandNotFound,
          .message = "command not registered: " + name,
      });
    }

    // Handler signature: fn(params, ctx)
    auto result = it->second(params, runtime->context);
    if (!result.valid()) {
      sol::error err = result;
      return std::unexpected(Error{
          .code    = ErrorCode::LuaError,
          .message = "command '" + name + "' failed",
          .details = err.what(),
      });
    }

    return result;
  }

  // ── View lifecycle dispatch ───────────────────────────────────────────

  void dispatchViewLifecycleEvent(
      Runtime*           runtime,
      const std::string& viewName,
      const std::string& eventName,
      sol::table         extraPayload
  ) {
    if (runtime == nullptr || runtime->lua_state == nullptr || runtime->context == nullptr) {
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
    payload["ctx"]     = runtime->context;
    sol::table props   = descriptor["_props"];
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
      debug::warn(
          std::format("lifecycle event '{}' for '{}' failed: {}", eventName, viewName, err.what())
      );
    }
  }

  // ── Entry-point loader ──────────────────────────────────────────────────

  std::expected<bool, Error> loadEntryPoint(Runtime* runtime, Config* cfg) {
    if (runtime == nullptr || runtime->lua_state == nullptr) {
      return std::unexpected(Error{
          .code    = ErrorCode::InvalidConfig,
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
          .code    = ErrorCode::LuaError,
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
            .code    = ErrorCode::LuaError,
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
          if (v.is<std::string>())
            field = v.as<std::string>();
        };
        auto mergeInt = [&](const char* key, int& field) {
          sol::object v = t[key];
          if (v.is<int>())
            field = v.as<int>();
        };

        mergeInt("window_width", cfg->window_width);
        mergeInt("window_height", cfg->window_height);
        mergeInt("window_min_width", cfg->window_min_width);
        mergeInt("window_min_height", cfg->window_min_height);
        mergeInt("window_max_width", cfg->window_max_width);
        mergeInt("window_max_height", cfg->window_max_height);
        mergeStr("initial_view", cfg->initial_view);
        mergeStr("title", cfg->title);
        mergeStr("view_root", cfg->view_root);
        mergeStr("fallback_file", cfg->fallback_file);
        mergeStr("asset_root", cfg->asset_root);
        mergeStr("command_root", cfg->command_root);

        // Merge views block from returned table.
        sol::object views = t["views"];
        if (views.is<sol::table>()) {
          for (auto& [k, v] : views.as<sol::table>()) {
            if (!v.is<sol::table>())
              continue;
            sol::table  vt   = v.as<sol::table>();
            std::string name = k.as<std::string>();
            std::string kind = vt["kind"].get_or<std::string>("");
            if (kind != "file" && kind != "html" && kind != "url")
              continue;
            cfg->views[name] = ViewEntry{
                .kind = std::move(kind),
                .src  = vt["src"].get_or<std::string>(""),
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
          sol::table vt       = views_obj.as<sol::table>();
          sol::table registry = lua.create_table();
          for (auto& [k, v] : vt) {
            if (!v.is<sol::table>())
              continue;
            sol::table  desc  = v.as<sol::table>();
            std::string name  = k.as<std::string>();
            std::string kind  = desc["kind"].get_or<std::string>("");
            std::string value = desc["value"].get_or<std::string>("");
            if (kind.empty())
              continue;
            cfg->views[name] = ViewEntry{.kind = std::move(kind), .src = std::move(value)};
            registry[name]   = desc;
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
      sol::object ctx_obj     = lua["ctx"];
      auto        cmds_result = cmds_fn.as<sol::function>()(ctx_obj);
      if (!cmds_result.valid()) {
        sol::error err = cmds_result;
        debug::warn(std::format("coconut.commands(ctx) failed: {}", err.what()));
      } else {
        debug::info("coconut.commands(ctx) applied");
      }
    }

    // ── Auto-load main-thread generated commands ──────────────────────
    // Scan the command root directory and the generated directory for
    // .g_mt.lua files.  Each .g_mt.lua exports a register(ctx) function
    // that calls ctx:bind_mt() for each command defined in the
    // corresponding .lua module with ---@thread main.
    //
    // Regular .g.lua files (without _mt) are loaded by the background
    // thread's own Lua state.
    {
      std::string cmdRoot = cfg ? cfg->command_root : "commands";
      std::string genDir  = "generated";
      debug::info(std::format("scanning {}/ and {}/ for .g_mt.lua files...", cmdRoot, genDir));

      // Add directories to package.path so require() works.
      std::string pkgPath = ";" + cmdRoot + "/?.lua;" + cmdRoot + "/?/init.lua;" + genDir +
                            "/?.lua;" + genDir + "/?/init.lua";
      lua.script("package.path = package.path .. '" + pkgPath + "'");

      sol::object ctx_obj = lua["ctx"];
      if (!ctx_obj.valid()) {
        debug::warn("ctx not available, skipping main-thread command auto-load");
      } else {
        int                      loaded     = 0;
        std::vector<std::string> dirsToScan = {cmdRoot, genDir};
        try {
          for (const auto& scanDir : dirsToScan) {
            if (!std::filesystem::is_directory(scanDir))
              continue;

            for (auto& entry : std::filesystem::directory_iterator(scanDir)) {
              auto path = entry.path();
              if (path.extension() != ".lua")
                continue;
              auto stem = path.stem().string();

              // Only load .g_mt.lua files (main-thread command registration).
              if (stem.size() < 5 || stem.substr(stem.size() - 5) != ".g_mt")
                continue;

              std::string cmdName = stem.substr(0, stem.size() - 5);
              debug::info(std::format("found {}.g_mt.lua, loading...", cmdName));

              // Load the .g_mt.lua file — it returns a register function.
              auto loadResult = lua.script_file(path.string(), sol::script_pass_on_error);
              if (!loadResult.valid()) {
                sol::error e = loadResult;
                debug::warn(std::format("failed to load {}: {}", path.filename().string(), e.what())
                );
                continue;
              }

              // The returned value should be the register function.
              sol::object ret = loadResult;
              if (!ret.is<sol::function>()) {
                debug::warn(std::format(
                    "{} did not return a function (returned type {})",
                    path.filename().string(),
                    static_cast<int>(ret.get_type())
                ));
                continue;
              }

              // Call register(ctx) — the .g_mt.lua uses ctx:bind_mt() internally.
              auto bindResult = ret.as<sol::function>()(ctx_obj);
              if (!bindResult.valid()) {
                sol::error e = bindResult;
                debug::warn(std::format("register({}) failed: {}", cmdName, e.what()));
              } else {
                ++loaded;
                debug::info(std::format("registered {} main-thread commands", cmdName));
              }
            }
          }
        } catch (const std::filesystem::filesystem_error& err) {
          debug::info(std::format("no {}/ or {}/ directory", cmdRoot, genDir));
        }
        if (loaded > 0) {
          debug::info(std::format("loaded {} main-thread command module(s)", loaded));
        }
      }
    }

    debug::info("loadEntryPoint done");
    return true;
  }

  void wireWindowHandle(Runtime* runtime) {
    if (runtime == nullptr || runtime->lua_state == nullptr || runtime->context == nullptr ||
        runtime->context->window_handle == nullptr) {
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

}  // namespace coconut::lua
