-- commands/math.g.lua
-- Tests numeric JSON round-trip through the worker.
return function(ctx)
  ctx:bind("add", function(params)
    local a = params.a or 0
    local b = params.b or 0
    return { result = a + b }
  end)

  ctx:bind("multiply", function(params)
    local a = params.a or 0
    local b = params.b or 0
    return { result = a * b }
  end)
end
