---@command hello
---@param params { name?: string }
---@return string
local function hello(params, ctx)
  return "Hi " .. ((params and params.name) or "user")
end

return {
  hello = hello,
}
