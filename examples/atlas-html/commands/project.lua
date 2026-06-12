-- Atlas Tool: project persistence commands
---@command project_list
---@return { projects: table }
local function project_list(_, ctx)
  local dir = "projects"
  if not coconut.fs.exists(dir) then
    return { projects = {} }
  end
  local items = coconut.fs.listDir(dir)
  local projects = {}
  for _, item in ipairs(items) do
    if not item.is_dir then
      local name = item.name:match("^(.*)%.json$")
      if name then
        table.insert(projects, name)
      end
    end
  end
  table.sort(projects)
  return { projects = projects }
end

---@command project_save
---@param params { name: string, data: string }
---@return { ok: boolean }
local function project_save(params, ctx)
  local name = params.name
  if not name or name == "" then
    return { ok = false, error = "no name" }
  end
  -- Sanitize name
  name = name:gsub("[^%w_-]", "_")
  local dir = "projects"
  if not coconut.fs.exists(dir) then
    coconut.fs.writeText(dir .. "/.gitkeep", "")
  end
  local path = dir .. "/" .. name .. ".json"
  local ok = coconut.fs.writeText(path, params.data or "{}")
  return { ok = ok }
end

---@command project_load
---@param params { name: string }
---@return { ok: boolean, data: string? }
local function project_load(params, ctx)
  local name = params.name
  if not name or name == "" then
    return { ok = false }
  end
  name = name:gsub("[^%w_-]", "_")
  local path = "projects/" .. name .. ".json"
  if not coconut.fs.exists(path) then
    return { ok = false }
  end
  local data = coconut.fs.readText(path)
  if data then
    return { ok = true, data = data }
  end
  return { ok = false }
end

---@command project_delete
---@param params { name: string }
---@return { ok: boolean }
local function project_delete(params, ctx)
  local name = params.name
  if not name or name == "" then
    return { ok = false }
  end
  name = name:gsub("[^%w_-]", "_")
  local path = "projects/" .. name .. ".json"
  if coconut.fs.exists(path) then
    os.remove(path)
    return { ok = true }
  end
  return { ok = false }
end

return {
  project_list = project_list,
  project_save = project_save,
  project_load = project_load,
  project_delete = project_delete,
}
