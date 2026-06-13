--- playground - test every Coconut Milk feature interactively.
---
--- Register extra commands for: env, json, custom-bind, window-ctl, fs_read_text

local json = coconut.json
local fs = coconut.fs

-- ── Connect / ping ──────────────────────────────────────────────────────

---@command ping
local function ping(params, ctx)
  return "pong"
end

---@command getViews
local function getViews(params, ctx)
  local names = {}
  for name, _ in pairs(coconut.views()) do
    table.insert(names, name)
  end
  return names
end

-- ── Window controls ─────────────────────────────────────────────────────

---@command __coconut_window_ctl
local function window_ctl(params, ctx)
  local win = _G.__playground_win or _coconut_window
  if not win then return { ok = false, error = "no window handle" } end
  _G.__playground_win = win
  local cmd = params.cmd
  if cmd == "debug" then
    return {
      win_type = type(win),
      has_minimize = type(win.minimize) == "function",
      has_resize = type(win.resize) == "function",
      has_setPosition = type(win.setPosition) == "function",
      has_setFullscreen = type(win.setFullscreen) == "function",
      has_reload = type(win.reload) == "function",
    }
  end
  if cmd == "minimize" then
    win:minimize()
  elseif cmd == "maximize" then
    win:maximize()
  elseif cmd == "toggleFullscreen" then
    win:toggleFullscreen()
  elseif cmd == "close" then
    win:close()
  elseif cmd == "fullscreen_on" then
    win:setFullscreen(true)
  elseif cmd == "fullscreen_off" then
    win:setFullscreen(false)
  elseif cmd == "resize" then
    win:resize(params.w or 800, params.h or 600)
  elseif cmd == "setPosition" then
    win:setPosition(params.x or 0, params.y or 0)
  elseif cmd == "reload" then
    win:reload()
  end
  return { ok = true }
end

-- ── Env info (Lua-side) ─────────────────────────────────────────────────

---@command playground_env
local function playground_env(params, ctx)
  return {
    HOME = coconut.env.HOME,
    USER = coconut.env.USER,
    cwd = coconut.env.cwd,
    homedir = coconut.env.homedir,
    pathSeparator = coconut.env.pathSeparator,
  }
end

-- ── JSON roundtrip test ─────────────────────────────────────────────────

---@command playground_json
local function playground_json(params, ctx)
  local payload = params.payload or "{}"
  local obj = json.parse(payload)
  obj._test_roundtrip = true
  obj._coconut = "milk"
  return json.jsonify(obj)
end

-- ── Echo (test custom bind) ──────────────────────────────────────────────

---@command playground_echo
local function playground_echo(params, ctx)
  return {
    echoed = params.value or "",
    received_at = os.date("%H:%M:%S"),
  }
end

-- ── File read (bridge-friendly) ─────────────────────────────────────────

---@command fs_read_text
local function fs_read_text(params, ctx)
  local path = params.path
  if not path or path == "" then
    return { ok = false, error = "no path given" }
  end
  local ok, data = pcall(fs.readText, path)
  if ok then
    return { ok = true, data = data }
  end
  return { ok = false, error = tostring(data) }
end

-- ── Frontend event test ──────────────────────────────────────────────────

---@command playground_send_event
local function playground_send_event(params, ctx)
  coconut.emit("playground_event", {
    message = params.message or "hello from Lua",
    count = params.count or 1,
  })
  return { ok = true }
end

-- ── Exports ──────────────────────────────────────────────────────────────

return {
  ping = ping,
  getViews = getViews,
  window_ctl = window_ctl,
  playground_env = playground_env,
  playground_json = playground_json,
  playground_echo = playground_echo,
  fs_read_text = fs_read_text,
  playground_send_event = playground_send_event,
}
