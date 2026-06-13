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

-- ── Startup smoke tests (logged but not shown in UI) ────────────────────

function coconut.on_ready()
  coconut.info("playground ready")
  coconut.info("HOME = " .. tostring(coconut.env.HOME))
  coconut.info("cwd  = " .. coconut.env.cwd)

  -- test clipboard read (silent)
  local cb = coconut.clipboard.readText()
  coconut.info("clipboard = " .. (#cb > 0 and cb:sub(1, 40) or "(empty)"))
end

-- ── Navigation handler ───────────────────────────────────────────────────

function coconut.events(name, payload, ctx)
  if name == "navigate" then
    ctx:show(payload.view)
  end
end
