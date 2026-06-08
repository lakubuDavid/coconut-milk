--- Coconut Milk application entry point.

--- Returns a table of named view descriptors.
function coconut.views()
  return {
    home = View.load("views/home.html"),
    note = View.load("views/note.html"),
    commands=View.load("views/commands.html"),
  }
end

--- Startup config hook.
--- The ctx setters mutate the shared Config in-place, merging app-level
--- overrides on top of the defaults from coconut.config.lua.
function coconut.config(ctx)
  ctx
      :setBrowser("auto")
      :setWindowSize({ w = 1280, h = 640 })
      :setInitialView("home")

  return ctx
end

--- Called when the window is resized.
function coconut.onResize(ctx, w, h)
  -- coconut.emit("on_resize", { w = w, h = h })
  print(w, h)
end

--- Love2D-like dispatcher for frontend → Lua events.
function coconut.events(name, payload, ctx)
  if name == "navigate" then
    ctx:show(payload.view)
  elseif name == "resize" then
    print(payload.w, payload.h)
  end
end

--- Register commands (called from ctx:bind).
function coconut.commands(ctx)
  ctx:bind("ping", function(params, ctx)
    return "pong"
  end)
end
