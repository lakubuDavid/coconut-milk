local View = require "coconut.view"

function coconut.views()
  local noteView = View.load("http://example.com")
  noteView
      :defineProps({
        data = {} -- fetch data online or load from disk (default props)
      })
      :on_mount(function(e)
        -- navigation props are available as e.props
        print("note mounted", e.props and e.props.data)
      end)
      :on_unmount(function(e)
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

coconut.on("resize", function(event)
  coconut.emit({ name = "resize_client", w = event.w, h = event.h })
end)

--- Last-resort fallback dispatcher.
--- Called after all view and subscriber handlers.
function coconut.events(event)
end

--- Later maybe
-- This should generate .d.ts

-- this command while be namede `sayHi` in the frontnd
-- !command(sayHi)
function SayHey(params)
  print("Hi " .. (params.name or "user"))
end
