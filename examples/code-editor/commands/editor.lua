-- File operations for the code editor.

local function is_image(path)
  local ext = path:match("%.([^%.]+)$")
  if not ext then return false end
  ext = ext:lower()
  return ext == "png" or ext == "jpg" or ext == "jpeg"
      or ext == "gif" or ext == "svg" or ext == "webp"
      or ext == "bmp" or ext == "ico"
end

local function text_type(path)
  local ext = path:match("%.([^%.]+)$")
  if not ext then return "text" end
  ext = ext:lower()
  local map = {
    lua = "lua", js = "javascript", ts = "javascript",
    css = "css", html = "xml", htm = "xml", xml = "xml",
    md = "markdown", json = "javascript",
    py = "python", rb = "python",
    c = "clike", cpp = "clike", h = "clike", hpp = "clike",
    java = "clike", rs = "rust", go = "go",
    yaml = "yaml", yml = "yaml", toml = "yaml",
    sh = "shell", bash = "shell", zsh = "shell",
    txt = "text", cfg = "text", conf = "text",
  }
  return map[ext] or "text"
end

-- List directory contents
local function list_dir(payload, ctx)
  local dir = payload.path
  if not dir or dir == "" then dir = "." end
  return coconut.fs.listDir(dir)
end

-- Read a file (text or image)
local function read_file(payload, ctx)
  local path = payload.path
  if not path or path == "" then
    return { error = "no path provided" }
  end
  if is_image(path) then
    return {
      type = "image",
      path = path,
      name = path:match("([^/\\]+)$") or path,
    }
  end
  local content = coconut.fs.readText(path)
  if content then
    return {
      content = content,
      type = "text",
      text_type = text_type(path),
      path = path,
      name = path:match("([^/\\]+)$") or path,
    }
  end
  return { error = "could not read file: " .. path }
end

-- Save content to a file
local function save_file(payload, ctx)
  if not payload.path or payload.content == nil then
    coconut.warn("save_file: missing path or content")
    return { error = "missing path or content" }
  end
  coconut.info("save_file: path=" .. payload.path .. " content_len=" .. #payload.content)
  local ok = coconut.fs.writeText(payload.path, payload.content)
  if ok then
    coconut.info("save_file: ok=true")
    return { ok = true }
  end
  coconut.warn("save_file: could not write file: " .. payload.path)
  return { error = "could not write file: " .. payload.path }
end

-- Show open dialog (files + folders)
local function open_dialog(payload, ctx)
  coconut.info("open_dialog: calling coconut.dialog.open")
  local result = coconut.dialog.open("Open File or Folder", false, true)
  coconut.info("open_dialog: confirmed=" .. tostring(result.confirmed) .. " path=" .. (result.path or "") .. " is_dir=" .. tostring(result.is_dir))
  if result.confirmed and result.path and result.path ~= "" then
    return { path = result.path, is_dir = result.is_dir }
  end
  return { cancelled = true }
end

-- Show save file dialog
local function save_dialog(payload, ctx)
  local default_name = payload.default_name or "untitled.txt"
  coconut.info("save_dialog: calling coconut.dialog.save default_name=" .. default_name)
  local result = coconut.dialog.save("Save File", default_name)
  coconut.info("save_dialog: confirmed=" .. tostring(result.confirmed) .. " path=" .. (result.path or ""))
  if result.confirmed and result.path and result.path ~= "" then
    return { path = result.path }
  end
  return { cancelled = true }
end

return {
  list_dir = list_dir,
  read_file = read_file,
  save_file = save_file,
  open_dialog = open_dialog,
  save_dialog = save_dialog,
}
