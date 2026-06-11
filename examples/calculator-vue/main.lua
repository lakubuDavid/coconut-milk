local dev = os.getenv("COCONUT_DEV") == "1"

function coconut.views()
  return {
    app = View.load("dist/index.html")
      :on_load(function(ctx)
        print("[calc] app view loaded")
      end)
      :on_mount(function(ctx)
        print("[calc] app view mounted")
      end)
      :on_unmount(function(ctx)
        print("[calc] app view unmounted")
      end),
    dev = View.url("http://localhost:5173")
      :on_mount(function(ctx)
        print("[calc] dev view mounted")
      end)
      :on_unmount(function(ctx)
        print("[calc] dev view unmounted")
      end),
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
