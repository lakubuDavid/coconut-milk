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
  return (params.a or 0) + (params.b or 0)
end

---@command login
---@description Authenticate a user with optional remember flag.
---@param username string
---@param password string
---@param remember? boolean
---@returntable
local function login(params, ctx)
  return { ok = true, token = "abc123" }
end

---@command search
---@description Full-text search with filter and pagination.
---@param query string
---@param options? { fuzzy?: boolean, limit?: number, offset?: number }
---@return string[]
local function search(params, ctx)
  return {}
end

---@command transform
---@description Apply a function to each element.
---@param data number[]
---@param fn fun(x: number): string
---@return string[]
local function transform(params, ctx)
  return {}
end

---@command merge
---@description Deep-merge two tables with union type for overrides.
---@param base { name: string, meta?: table }
---@param overrides { name?: string, meta?: table } | nil
---@return { name: string, meta?: table }
local function merge(params, ctx)
  return { name = "merged" }
end

---@command evaluate
---@description Evaluate an expression with a typed callback.
---@param expr string | number | boolean
---@param validate fun(val: string | number): boolean
---@return { result: string | number | boolean, valid: boolean }
local function evaluate(params, ctx)
  return { result = params.expr, valid = true }
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
