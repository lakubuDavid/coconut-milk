local dev = os.getenv("COCONUT_DEV") == "1"

function coconut.views()
  return {
    app = View.load("dist/index.html"),
    dev = View.url("http://localhost:5173"),
  }
end

function coconut.config(ctx)
  ctx
    :setWindowSize({ w = 480, h = 700 })
    :setTitle("Calculator")
    :setResizable(false)
    :setFrameless(true)
    :setInitialView(dev and "dev" or "app")
  return ctx
end
