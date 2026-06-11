local dev = os.getenv("COCONUT_DEV") == "1"
return {
  initial_view = dev and "dev" or "app",
  view_root = "views",
  command_root = "commands",
  views = {
    app = { kind = "file", src = "dist/index.html" },
    dev = { kind = "url", src = "http://localhost:5173" },
  },
}
