--- playground - test every Coconut Milk feature interactively.
---
--- These are extra commands beyond the built-in ones (ping, getViews,
--- fs_read_text, __coconutWindowCtl, clipboard_*, openUrl, notify,
--- dialog_*, fs_*).

local json = coconut.json

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
  playground_env = playground_env,
  playground_json = playground_json,
  playground_echo = playground_echo,
  playground_send_event = playground_send_event,
}
