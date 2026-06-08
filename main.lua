--- Coconut Milk application entry point.
---
--- Defines the application callbacks that the framework calls after
--- loading the base config (coconut.config.lua) and setting up the runtime.

--- Returns a table of named view descriptors.
--- TODO: populate with real View.url/html/load calls once View methods are bound.
function coconut.views()
  return {}
end

--- Startup config hook.
--- Receives the runtime ctx object and uses chainable setters to
--- override config-file settings (browser, window size, initial view).
---
--- The setters mutate the shared Config in-place, merging app-level
--- overrides on top of the defaults from coconut.config.lua.
function coconut.config(ctx)
  ctx
    :setBrowser("auto")
    :setWindowSize({ w = 1280, h = 640 })
    :setInitialView("home")

  return ctx
end

--- Called when the window is resized.
function coconut.on_resize(ctx, w, h)
  coconut.emit("on_resize", { w = w, h = h })
end

--- Love2D-like dispatcher for frontend → Lua events.
function coconut.events(name, payload, ctx)
end
