return {
  browser = "webview",
  window_width = 1280,
  window_height = 720,
  frameless = true,
  initial_view = "home",
  view_root = "views",
  asset_root = "assets",
  command_root = "samples/commands",
  generators = {
    output_dir = "generated"
  },
  views = {
    home = { kind = "file", src = "views/home.html" },
    note = { kind = "file", src = "views/note.html" },
    about = { kind = "html", src = "<h1>About Coconut</h1><p>A minimal Lua desktop UI framework.</p>" },
    external = { kind = "url", src = "https://example.com" }
  }
}
