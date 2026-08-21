-- commands/echo.g.lua
-- Simple pass-through command for testing the worker pipeline.
return function(ctx)
  ctx:bind("echo", function(params)
    local msg = params.message or ""
    return { echoed = msg }
  end)
end
