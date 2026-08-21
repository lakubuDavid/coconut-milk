-- commands/slow.g.lua
-- Commands that simulate real work, for testing shutdown abort modes.
return function(ctx)
  ctx:bind("sleep_ms", function(params)
    local ms = params.ms or 100
    ctx.sleep(ms)  -- blocks the worker thread for real
    return { delayed_ms = ms, done = true }
  end)
end
