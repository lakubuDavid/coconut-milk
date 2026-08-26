#include "builtins.h"

#include "../debug.h"

#include <format>

namespace coconut::modules {

  // Framework-level bind_mt commands, previously an inline Lua string in
  // main_runtime.cpp. They wrap the C++ functions bound on the coconut
  // table so the JS bridge can dispatch to them from the sync executor.
  void init_builtins(sol::state& lua, ThreadKind kind) {
    if (kind != ThreadKind::Main) {
      return;  // bind_mt registries are a main-thread concept
    }

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
    -- run loop via core::dispatchPost. Single API, correct dispatch either way.
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

}  // namespace coconut::modules
