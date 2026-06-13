return {
  app = {
    name = "Coconut Playground",
    id = "com.coconut-milk.playground",
    version = "0.1.0",
    description = "Interactive test bench for every Coconut Milk feature.",
    category = "developer-tools",
  },

  -- Global window defaults
  window_width = 1280,
  window_height = 720,
  frameless = false,
  transparent = false,
  resizable = true,
  initial_view = "app",
  view_root = "views",
  asset_root = "assets",
  command_root = "commands",

  debug = {
    enabled = true,
    logLevel = "info",
  },

  -- Platform-specific window style overrides
  darwin = {
    -- macOS uses transparent webview by default
    transparent = true,
    -- Bundle identity
    bundle_identifier = "com.coconut-milk.playground",
    -- Notification permission strings
    ns = {
      notification_alert_style = "alert",
      usage_descriptions = {
        NSCameraUsageDescription = "Camera access is needed for the scan panel",
        NSMicrophoneUsageDescription = "Microphone access is needed for voice commands",
        NSContactsUsageDescription = "Contacts access is needed for sharing",
      },
    },
  },

  win = {
    dpi_awareness = "per-monitor-v2",
    requested_privileges = "asInvoker",
    long_paths = true,
    app = {
      id = "com.coconut-milk.playground",
    },
  },

  linux = {
    categories = "Development;IDE;Utility;",
    app = {
      id = "com.coconut-milk.playground",
    },
  },

  views = {
    app = { kind = "file", src = "views/index.html" },
  },
}
