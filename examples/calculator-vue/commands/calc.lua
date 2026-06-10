-- Calculator history commands for the Vue calculator app.
-- Saves/loads calculation history to a JSON file.

local json = coconut.json
local fs = coconut.fs

local HISTORY_FILE = "calc_history.json"

---@command calc_save
---@param params { entries: { expr: string, result: string }[] }
---@return { ok: boolean }
local function calc_save(params, ctx)
  local data = json.jsonify({ entries = params.entries or {} })
  local ok = fs.writeText(HISTORY_FILE, data)
  return { ok = ok }
end

---@command calc_load
---@return { entries: { expr: string, result: string }[] }
local function calc_load(params, ctx)
  if not fs.exists(HISTORY_FILE) then
    return { entries = {} }
  end
  local content = fs.readText(HISTORY_FILE)
  local ok, data = pcall(json.parse, content)
  if ok and type(data) == "table" and type(data.entries) == "table" then
    return data
  end
  return { entries = {} }
end

return {
  calc_save = calc_save,
  calc_load = calc_load,
}
