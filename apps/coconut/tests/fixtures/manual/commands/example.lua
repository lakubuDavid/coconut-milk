--- Example command for the Coconut Milk app.
--- Demonstrates @command annotations used by the code generator.

local json = coconut.json
local fs = coconut.fs

---@command ping
---@return { message: string }
local function ping(params, ctx)
  return { message = "pong" }
end

---@command greet
---@param params { name: string }
---@return { greeting: string }
local function greet(params, ctx)
  local name = params.name or "world"
  return { greeting = "Hello, " .. name .. "!" }
end

return {
  ping = ping,
  greet = greet,
}
