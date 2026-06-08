-- LuaLS annotations + Coconut @command tag
---@command hello
---@description My command description
--- Somthing 
---@param arg0 { name?: string }
---@return string
local function hello(arg0, ctx)
  local name = (arg0 and arg0.name) or "user"
  local message = "Hi " .. name

  if ctx and ctx.emit then
    coconut.emit("greeted", { name = name })
  end

  return message
end

---@command hi
---@description My command description
---@param name string
---@param ctx {name?: string }
---@return string
local function hey(arg0, ctx)
  local name = (arg0 and arg0.name) or "user"
  local message = "Hi " .. name

  if ctx and ctx.emit then
    coconut.emit("greeted", { name = name })
  end

  return message
end

return {
  hello = hello,
  hi=hey
}
