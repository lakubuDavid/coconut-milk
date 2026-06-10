--- Coconut Milk application entry point.

--- Returns a table of named view descriptors.
function coconut.views()
  return {
    home = View.load("views/home.html"),
    note = View.load("views/note.html"),
    commands=View.load("views/commands.html"),
    settings=View.load("views/settings.html"),
    external = View.url("https://lakubudavid.me"),
  }
end

--- Startup config hook.
--- The ctx setters mutate the shared Config in-place, merging app-level
--- overrides on top of the defaults from coconut.config.lua.
function coconut.config(ctx)
  ctx
      :setWindowSize({ w = 1280, h = 640 })
      :setTitle("Coconut Milk")
      :setResizable(true)
      :setMinimumWindowSize({ w = 640, h = 320 })
      :setMaximumWindowSize({ w = 2560, h = 1440 })
      :setFrameless(false)
      :setTransparent(true)
      :setInitialView("home")

  return ctx
end

--- Love2D-like dispatcher for frontend → Lua events.
function coconut.events(name, payload, ctx)
  coconut.info("event: " .. name)

  if name == "navigate" then
    ctx:show(payload.view)
  elseif name == "move" then
    ctx.window:move({ x = payload.dx, y = -payload.dy })
  end
end

--- Register commands (called from ctx:bind).
function coconut.commands(ctx)
  ctx:bind("ping", function(params, ctx)
    return "pong"
  end)

  -- Get all registered view names (for dynamic navigation)
  ctx:bind("getViews", function(params, ctx)
    local views = coconut.views()
    local names = {}
    for name, _ in pairs(views) do
      table.insert(names, name)
    end
    return names
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

  -- Filesystem demo: read a text file
  ctx:bind("fs_read_text", function(params, ctx)
    local path = params.path
    if not path or path == "" then
      return { ok = false, error = "no path provided" }
    end
    local ok, content = pcall(coconut.fs.readText, path)
    if ok then
      return { ok = true, data = content }
    else
      return { ok = false, error = tostring(content) }
    end
  end)
end
