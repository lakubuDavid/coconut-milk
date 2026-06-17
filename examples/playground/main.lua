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
-- ── Window handle diagnostics ───────────────────────────────────────────

local w = _coconut_window
coconut.info("_coconut_window type = " .. type(w))
if w then
  coconut.info("  .minimize = " .. type(w.minimize))
  coconut.info("  .resize   = " .. type(w.resize))
  coconut.info("  .setPosition = " .. type(w.setPosition))
end

-- ── Navigation handler ───────────────────────────────────────────────────

function coconut.events(event)
  if event.name == "navigate" then
    ctx:show(event.view)
  end
end
