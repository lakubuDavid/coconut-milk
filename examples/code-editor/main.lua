-- code-editor: A simple code editor for Coconut Milk.
-- Features: file tree, CodeMirror editing, image preview, native dialogs.
--
-- Commands use the @command annotation and are auto-registered via the
-- generated .g.lua wrapper.  The frontend imports generated .g.js
-- wrappers instead of calling coconut.call() directly.

package.path = "lib/?.lua;" .. package.path

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

function coconut.events(event)
  if event.name == "navigate" then
    ctx:show(event.view)
  end
end
