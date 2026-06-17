-- LuaLS annotations + Coconut @command tag
---@command hello
---@param params { name?: string }
---@return string
local function hello(params, ctx)
  local name = (params and params.name) or "user"
  if ctx and ctx.emit then
    coconut.emit({ name = "greeted", user = name })
  end
  return "Hi " .. name
end

---@command goodbye
---@param params { name?: string }
---@return string
local function goodbye(params, ctx)
  local name = (params and params.name) or "user"
  if ctx and ctx.emit then
    coconut.emit({ name = "farewell", user = name })
  end
  return "Bye " .. name
end

return {
  hello = hello,
  goodbye = goodbye,
}
