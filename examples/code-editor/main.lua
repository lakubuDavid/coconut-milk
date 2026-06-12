-- code-editor: A simple code editor for Coconut Milk.
-- Features: file tree, CodeMirror editing, image preview, native dialogs.

package.path = "lib/?.lua;" .. package.path

local editor = require("commands.editor")

-- ── Register all editor commands ────────────────────────────────────────

function coconut.commands(ctx)
  ctx:bind("editor_list_dir", editor.list_dir)
  ctx:bind("editor_read_file", editor.read_file)
  ctx:bind("editor_save_file", editor.save_file)
  ctx:bind("editor_open_dialog", editor.open_dialog)
  ctx:bind("editor_save_dialog", editor.save_dialog)
end

-- ── Views ───────────────────────────────────────────────────────────────

function coconut.views()
  return {
    workspace = View.load("views/workspace.html"),
  }
end

-- ── Config ──────────────────────────────────────────────────────────────

function coconut.config(ctx)
  ctx
    :setWindowSize({ w = 1100, h = 700 })
    :setTitle("Coconut Code Editor")
    :setResizable(true)
    :setInitialView("workspace")
  return ctx
end

-- ── Events ──────────────────────────────────────────────────────────────

function coconut.events(name, payload, ctx)
  if name == "navigate" then
    ctx:show(payload.view)
  end
end
