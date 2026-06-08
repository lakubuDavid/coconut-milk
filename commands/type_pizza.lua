-- Sample command module exercising various LuaCATS annotation styles
-- to stress-test the type parser and code generators.

---@command noop
---@description Does nothing.  No params, no meaningful return.
local function noop(params, ctx)
  return true
end

---@command ping
---@return string
local function ping(params, ctx)
  return "pong"
end

---@command greet
---@description Say hello
---@param name string
---@return string
local function greet(params, ctx)
  return "Hello, " .. (params.name or "world") .. "!"
end

---@command sum
---@description Add two numbers.
--- A multi-line description that spans
--- several lines to test continuation parsing.
---@param a number
---@param b number
---@return number
local function sum(params, ctx)
  local a = tonumber(params.a) or 0
  local b = tonumber(params.b) or 0
  return a + b
end

---@command login
---@description Authenticate a user with optional remember flag.
---@param username string
---@param password string
---@param remember? boolean
---@returntable
local function login(params, ctx)
  local ok = params.username ~= nil and params.username ~= "" and
             params.password ~= nil and params.password ~= ""
  if not ok then
    return { ok = false, error = "username and password are required" }
  end
  return {
    ok = true,
    token = "tok_" .. params.username .. "_" .. os.time(),
    remember = params.remember == true,
  }
end

---@command search
---@description Full-text search with filter and pagination.
---@param query string
---@param options? { fuzzy?: boolean, limit?: number, offset?: number }
---@return string[]
local function search(params, ctx)
  local limit = 10
  local offset = 0
  local fuzzy = false
  if params.options then
    limit = tonumber(params.options.limit) or 10
    offset = tonumber(params.options.offset) or 0
    fuzzy = params.options.fuzzy == true
  end

  local results = {}
  local q = (params.query or ""):lower()
  if q ~= "" then
    for i = 1, limit do
      local prefix = fuzzy and "~" or ""
      table.insert(results, prefix .. "result_" .. (offset + i) .. "_for_" .. q)
    end
  end
  return results
end

---@command transform
---@description Apply a function to each element.
---@param data number[]
---@param fn fun(x: number): string
---@return string[]
local function transform(params, ctx)
  local data = params.data or {}
  local fn = params.fn
  local out = {}
  for _, v in ipairs(data) do
    table.insert(out, tostring(v))
  end
  return out
end

---@command merge
---@description Deep-merge two tables with union type for overrides.
---@param base { name: string, meta?: table }
---@param overrides { name?: string, meta?: table } | nil
---@return { name: string, meta?: table }
local function merge(params, ctx)
  local base = params.base or {}
  local overrides = params.overrides or {}

  local function deep_merge(a, b)
    local out = {}
    for k, v in pairs(a) do out[k] = v end
    for k, v in pairs(b) do
      if type(v) == "table" and type(out[k]) == "table" then
        out[k] = deep_merge(out[k], v)
      else
        out[k] = v
      end
    end
    return out
  end

  return deep_merge(base, overrides)
end

---@command evaluate
---@description Evaluate an expression with a typed callback.
---@param expr string | number | boolean
---@param validate fun(val: string | number): boolean
---@return { result: string | number | boolean, valid: boolean }
local function evaluate(params, ctx)
  local expr = params.expr
  local result
  local kind = type(expr)
  if kind == "string" then
    local n = tonumber(expr)
    if n then
      result = n
    elseif expr == "true" then
      result = true
    elseif expr == "false" then
      result = false
    else
      result = expr
    end
  else
    result = expr
  end
  return { result = result, valid = true }
end

return {
  noop = noop,
  ping = ping,
  greet = greet,
  sum = sum,
  login = login,
  search = search,
  transform = transform,
  merge = merge,
  evaluate = evaluate,
}
