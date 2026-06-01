local View = require "coconut.view"

function coconut.views()
  local views = {
    home = View.url("https://example.com"),
    hello = View.html("<html><body>Hello</body></html>"),
    note = function()
      local view = View.load("note.html")
      -- do something
      return view
    end
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
      :bind("sayHi", function(params)
        print("Hi " .. (params.name or "user"))
      end)

  return ctx
end

function coconut.on_resize(w, h)
  ctx:emit("on_resize", { w = w, h = h })
end

--- Later maybe
-- This should generate .d.ts

-- !command(sayHi)
function SayHey(params)
  print("Hi " .. (params.name or "user"))
end
