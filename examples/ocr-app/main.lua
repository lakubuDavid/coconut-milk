function coconut.views()
  return {
    scan = View.load("views/scan.html")
      :on_load(function(ctx)
        print("[ocr] scan view loaded")
      end)
      :on_mount(function(ctx)
        print("[ocr] scan view mounted")
      end)
      :on_unmount(function(ctx)
        print("[ocr] scan view unmounted")
      end),
  }
end

function coconut.config(ctx)
  ctx
    :setWindowSize({ w = 900, h = 700 })
    :setMinimumWindowSize({ w = 600, h = 500 })
    :setTitle("OCR Scanner")
    :setResizable(true)
    :setInitialView("scan")
  return ctx
end

function coconut.on_resize(ctx, w, h)
  ctx:emit("window_resized", { w = w, h = h })
end
