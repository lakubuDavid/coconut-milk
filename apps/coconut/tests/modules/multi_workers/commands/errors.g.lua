-- commands/errors.g.lua
-- Commands that intentionally fail, for testing error handling.
return function(ctx)
  ctx:bind("fail_explicit", function(params)
    error(params.message or "intentional failure")
  end)

  ctx:bind("fail_nil_access", function(params)
    local t = nil
    return t.field  -- will raise a Lua error
  end)
end
