return {
  app = {
    name = "Coconut Code Editor",
    id = "com.coconut-milk.code-editor",
    version = "0.1.0",
    description = "A Lua-powered code editor built with Coconut Milk.",
    category = "developer-tools",
  },

  -- Global window defaults
  window_width = 1100,
  window_height = 700,
  resizable = true,
  title = "Coconut Code Editor",
  initial_view = "workspace",
  command_root = "commands",
  generators = {
    output_dir = "generated",
  },

  -- Platform-specific overrides
  darwin = {
    bundle_identifier = "com.coconut-milk.code-editor",
    ns = {
      notification_alert_style = "alert",
      usage_descriptions = {
        NSMicrophoneUsageDescription = "Microphone access for voice commands",
        NSCameraUsageDescription = "Camera access for video calls",
      },
    },
    -- macOS gets a slight transparency
    transparent = true,
  },

  win = {
    dpi_awareness = "per-monitor-v2",
    requested_privileges = "asInvoker",
    long_paths = true,
    app = {
      id = "com.coconut-milk.code-editor",
    },
  },

  linux = {
    categories = "Development;IDE;TextEditor;",
    app = {
      id = "com.coconut-milk.code-editor",
    },
  },

  views = {},
}
