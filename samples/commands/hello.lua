-- LuaLS annotations + Coconut @command tag
---@command hello
---@param params { name?: string }
---@return string
local function hello(params, ctx)
  local name = (params and params.name) or "user"
  local message = "Hi " .. name

  if ctx and ctx.emit then
    ctx:emit("greeted", { name = name })
  end

  return message
end

return {
  hello = hello,
}
