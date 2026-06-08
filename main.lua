--- Coconut Milk application entry point.

--- Returns a table of named view descriptors.
function coconut.views()
  return {
    home = View.load("views/home.html"),
    about = View.html([[<h1>About</h1><p>A minimal Lua desktop UI framework.</p>]]),
    commands = View.load("views/commands.html"),
  }
end

--- Startup config hook.
function coconut.config(ctx)
  ctx
    :setBrowser("auto")
    :setWindowSize({ w = 1280, h = 640 })
    :setInitialView("home")

  return ctx
end

--- Love2D-like dispatcher for frontend → Lua events.
function coconut.events(name, payload, ctx)
  if name == "navigate" then
    ctx:show(payload.view)
  end
end

--- Called when the window is resized.
function coconut.on_resize(ctx, w, h)
  coconut.emit("on_resize", { w = w, h = h })
end
