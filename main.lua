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
  coconut.info("event: " .. name .. " " .. vjson.encode(payload))

  if name == "navigate" then
    ctx:show(payload.view)
  elseif name == "resize" then
    print(payload.w, payload.h)
  elseif name == "grab_start" then
    coconut.warn("GRAB: window grab started")
  elseif name == "move" then
    ctx.window:move({ x = payload.dx, y = payload.dy })
  elseif name == "grab_end" then
    coconut.warn("GRAB: window grab ended")
  end
end

--- Register commands (called from ctx:bind).
function coconut.commands(ctx)
  ctx:bind("ping", function(params, ctx)
    return "pong"
  end)

  -- Window control commands for frameless-titlebar buttons
  ctx:bind("__coconut_window_ctl", function(params, ctx)
    local cmd = params.cmd
    coconut.info("window_ctl: " .. cmd)
    if cmd == "minimize" then
      ctx.window:minimize()
    elseif cmd == "toggleFullscreen" then
      ctx.window:toggleFullscreen()
    elseif cmd == "close" then
      ctx.window:close()
    end
  end)
end
