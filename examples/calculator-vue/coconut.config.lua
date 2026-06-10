-- Use COCONUT_DEV=1 to load from Vite dev server, otherwise loads built HTML.
local dev = os.getenv("COCONUT_DEV") == "1"

return {
  browser = "webview",
  window_width = 480,
  window_height = 700,
  resizable = false,
  title = "Calculator",
  initial_view = dev and "dev" or "app",
  view_root = "views",
  command_root = "commands",
  views = {
    app = { kind = "file", src = "views/app.html" },
    dev = { kind = "url", src = "http://localhost:5173" },
  },
}
