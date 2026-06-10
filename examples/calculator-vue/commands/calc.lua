local json = coconut.json
local fs = coconut.fs
local FILE = "calc_history.json"

---@command calc_save
---@param params { entries: { expr: string, result: string }[] }
---@return { ok: boolean }
local function calc_save(params, ctx)
  local data = json.jsonify({ entries = params.entries or {} })
  return { ok = fs.writeText(FILE, data) }
end

---@command calc_load
---@return { entries: { expr: string, result: string }[] }
local function calc_load(params, ctx)
  if not fs.exists(FILE) then return { entries = {} } end
  local ok, data = pcall(json.parse, fs.readText(FILE))
  if ok and type(data) == "table" and type(data.entries) == "table" then return data end
  return { entries = {} }
end

return { calc_save = calc_save, calc_load = calc_load }
