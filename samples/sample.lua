local View = require "coconut.view"

function coconut.views()
  local noteView = View.load("http://example.com")
  noteView
      :defineProps({
        data = {} -- fetch data online or load from disk (default props)
      })
      :on_mount(function(ctx)
        -- navigation props are available as ctx.props
        print("note mounted", ctx.props and ctx.props.data)
      end)
      :on_unmount(function(ctx)
        print("note unmounted")
      end)

  local views = {
    home = View.url("https://example.com"),
    hello = View.html("<html><body>Hello</body></html>"),
    note = noteView
  }
  return views
end

function coconut.config(ctx)
  ctx
      :setBrowser("auto")
      :setWindowSize({
        w = 1280,
        h = 640
      })
      :setInitialView("home")

  return ctx
end

function coconut.on_resize(ctx, w, h)
  coconut.emit("on_resize", { w = w, h = h })
end

--- Love2D-like dispatcher for frontend → Lua events.
--- Called for every `coconut.emit(...)` from the frontend.
function coconut.events(name, payload, ctx)
end

--- Later maybe
-- This should generate .d.ts

-- this command while be namede `sayHi` in the frontnd
-- !command(sayHi)
function SayHey(params)
  print("Hi " .. (params.name or "user"))
end
