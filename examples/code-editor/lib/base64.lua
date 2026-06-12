-- Minimal base64 encoder for LuaJIT.
-- Uses LuaJIT's bit library for bitwise operations.
local bit = require("bit")
local b64chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"

local function encode(data)
  local b64 = {}
  local i = 1
  local n = #data
  while i <= n do
    local a = string.byte(data, i)
    local b = string.byte(data, i + 1) or 0
    local c = string.byte(data, i + 2) or 0

    local char1 = bit.rshift(a, 2)
    local char2 = bit.bor(bit.lshift(bit.band(a, 3), 4), bit.rshift(b, 4))
    local char3 = bit.bor(bit.lshift(bit.band(b, 15), 2), bit.rshift(c, 6))
    local char4 = bit.band(c, 63)

    b64[#b64 + 1] = string.sub(b64chars, char1 + 1, char1 + 1)
    b64[#b64 + 1] = string.sub(b64chars, char2 + 1, char2 + 1)

    if i + 1 > n then
      b64[#b64 + 1] = "=="
      break
    end
    b64[#b64 + 1] = string.sub(b64chars, char3 + 1, char3 + 1)
    if i + 2 > n then
      b64[#b64 + 1] = "="
      break
    end
    b64[#b64 + 1] = string.sub(b64chars, char4 + 1, char4 + 1)
    i = i + 3
  end
  return table.concat(b64)
end

return { encode = encode }
