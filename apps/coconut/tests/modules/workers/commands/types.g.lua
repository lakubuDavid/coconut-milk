-- commands/types.g.lua
-- Tests JSON type round-trip: strings, bools, arrays, nested objects.
return function(ctx)
  ctx:bind("echo_types", function(params)
    return {
      str    = params.str or "",
      num    = params.num or 0,
      flag   = params.flag or false,
      list   = params.list or {},
      nested = params.nested or {},
    }
  end)

  ctx:bind("build_array", function(params)
    local n = params.count or 5
    local arr = {}
    for i = 1, n do
      arr[i] = "item_" .. i
    end
    return { items = arr }
  end)
end
