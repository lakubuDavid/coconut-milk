-- Notes persistence module.
-- Saves/loads notes to/from a plain text file (one note per line).

local notes_file = "notes_data.txt"

---@command notes_list
---@description Load all saved notes from disk.
---@return string[]
local function notes_list(params, ctx)
  local notes = {}
  local f = io.open(notes_file, "r")
  if f then
    for line in f:lines() do
      table.insert(notes, line)
    end
    f:close()
  end
  return notes
end

---@command notes_save
---@description Save notes to disk.
---@param notes string[]
local function notes_save(params, ctx)
  local notes = params.notes or {}
  local f, err = io.open(notes_file, "w")
  if not f then
    return { ok = false, error = "failed to open file: " .. (err or "unknown") }
  end
  for _, note in ipairs(notes) do
    f:write(note, "\n")
  end
  f:close()
  return { ok = true, count = #notes }
end

return {
  notes_list = notes_list,
  notes_save = notes_save,
}
