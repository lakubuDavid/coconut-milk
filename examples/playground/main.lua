--- playground: an interactive Coconut Milk feature tester.
---
--- Each feature (env, clipboard, notify, dialog, fs, window, json, events)
--- is exercised via the JS frontend. Some Lua-side tests run on startup.

function coconut.config(ctx)
  ctx
    :setWindowSize({ w = 900, h = 700 })
    :setTitle("Coconut Milk Playground")
    :setResizable(true)
    :setInitialView("app")
  return ctx
end

function coconut.views()
  return {
    app = View.load("views/index.html"),
  }
end

-- ── Startup smoke tests ─────────────────────────────────────────────────

local w = _coconut_window
coconut.info("_coconut_window type = " .. type(w))
if w then
  coconut.info("_coconut_window.minimize = " .. type(w.minimize))
  coconut.info("_coconut_window.resize = " .. type(w.resize))
  coconut.info("_coconut_window.setPosition = " .. type(w.setPosition))
  -- try calling minimize to see if it works
  local ok, err = pcall(function() w:minimize() end)
  coconut.info("w:minimize() = " .. tostring(ok) .. ", err=" .. tostring(err))
  ok, err = pcall(function() w:resize(400, 300) end)
  coconut.info("w:resize(400,300) = " .. tostring(ok) .. ", err=" .. tostring(err))
end

-- ── Navigation handler ───────────────────────────────────────────────────

function coconut.events(name, payload, ctx)
  if name == "navigate" then
    ctx:show(payload.view)
  end
end
