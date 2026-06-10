--- Calculator Vue app entry point.

function coconut.views()
  local dev = os.getenv("COCONUT_DEV") == "1"
  return {
    app = View.load("views/app.html"),
    dev = View.url("http://localhost:5173"),
  }
end

function coconut.config(ctx)
  ctx
    :setWindowSize({ w = 480, h = 700 })
    :setTitle("Calculator")
    :setResizable(false)
    :setFrameless(true)
    :setInitialView((os.getenv("COCONUT_DEV") == "1") and "dev" or "app")
  return ctx
end

function coconut.events(name, payload, ctx)
  -- no runtime events needed
end
