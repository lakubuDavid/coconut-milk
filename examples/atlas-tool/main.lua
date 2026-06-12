-- Atlas Tool — Sprite Atlas Packer + Tileset Resizer
-- A Coconut Milk example app

function coconut.config(ctx)
  return ctx
    :setWindowSize({ w = 1280, h = 760 })
    :setMinimumWindowSize({ w = 900, h = 600 })
    :setTitle("Atlas Tool")
    :setInitialView("app")
end

function coconut.views()
  return {
    app = View.load("views/app.html"),
  }
end
