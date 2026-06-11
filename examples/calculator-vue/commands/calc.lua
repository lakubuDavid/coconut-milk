local json = coconut.json
local fs = coconut.fs
local HISTORY_FILE = "calc_history.json"
local SETTINGS_FILE = "calc_settings.json"

---@command calc_save
---@param params { entries: { expr: string, result: string }[] }
---@return { ok: boolean }
local function calc_save(params, ctx)
  local data = json.jsonify({ entries = params.entries or {} })
  return { ok = fs.writeText(HISTORY_FILE, data) }
end

---@command calc_load
---@return { entries: { expr: string, result: string }[] }
local function calc_load(params, ctx)
  if not fs.exists(HISTORY_FILE) then return { entries = {} } end
  local ok, data = pcall(json.parse, fs.readText(HISTORY_FILE))
  if ok and type(data) == "table" and type(data.entries) == "table" then return data end
  return { entries = {} }
end

---@command settings_save
---@param params { theme: string, precision: number, sound: boolean }
---@return { ok: boolean }
local function settings_save(params, ctx)
  local data = json.jsonify({
    theme = params.theme or "mint",
    precision = params.precision or 2,
    sound = params.sound or false,
  })
  return { ok = fs.writeText(SETTINGS_FILE, data) }
end

---@command settings_load
---@return { theme?: string, precision?: number, sound?: boolean }
local function settings_load(params, ctx)
  if not fs.exists(SETTINGS_FILE) then return {} end
  local ok, data = pcall(json.parse, fs.readText(SETTINGS_FILE))
  if ok and type(data) == "table" then return data end
  return {}
end

return {
  calc_save = calc_save,
  calc_load = calc_load,
  settings_save = settings_save,
  settings_load = settings_load,
}
