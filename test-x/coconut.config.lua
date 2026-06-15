return {
  browser = "webview",
  initial_view = "app",
  view_root = "views",
  command_root = "commands",
  icon = {
    source = "assets/icon.svg",
  },
  views = {
    app = { kind = "file", src = "views/index.html" },
  },
}
