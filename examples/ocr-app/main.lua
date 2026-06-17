function coconut.views()
  return {
    scan = View.load("views/scan.html")
      :on_load(function(e)
        print("[ocr] scan view loaded")
      end)
      :on_mount(function(e)
        print("[ocr] scan view mounted")
      end)
      :on_unmount(function(e)
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

function coconut.events(event)
  if event.name == "window_resized" then
    -- forward to frontend
    ctx:emit({ name = "window_resized", w = event.w, h = event.h })
  end
end

coconut.on("resize", function(event)
  ctx:emit({ name = "window_resized", w = event.w, h = event.h })
end)
