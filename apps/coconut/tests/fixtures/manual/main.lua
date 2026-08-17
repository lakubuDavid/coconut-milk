--- test-x - Coconut Milk entry point.

function coconut.config(ctx)
  ctx
    :setWindowSize({ w = 960, h = 640 })
    :setTitle("test-x")
    :setResizable(true)
    :setInitialView("app")
  return ctx
end

function coconut.views()
  return {
    app = View.load("views/index.html"),
  }
end
