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
  local cmd = params.cmd
  if cmd == "minimize" then
    ctx.window:minimize()
  elseif cmd == "maximize" then
    ctx.window:maximize()
  elseif cmd == "toggleFullscreen" then
    ctx.window:toggleFullscreen()
  elseif cmd == "close" then
    ctx.window:close()
  elseif cmd == "fullscreen_on" then
    ctx.window:setFullscreen(true)
  elseif cmd == "fullscreen_off" then
    ctx.window:setFullscreen(false)
  elseif cmd == "resize" then
    ctx.window:resize({ w = params.w or 800, h = params.h or 600 })
  elseif cmd == "setPosition" then
    ctx.window:setPosition(params.x or 0, params.y or 0)
  elseif cmd == "reload" then
    ctx.window:reload()
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
