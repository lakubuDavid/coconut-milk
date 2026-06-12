-- lua-html: A pure Lua DSL for generating HTML.
-- From https://riki.house/lua-html — 117 lines.
-- Vendored here for the lua-html-app example.

local html = {}
setmetatable(html, html)

local escape_subs = {
  ["&"] = "&amp;",
  ["<"] = "&lt;",
  [">"] = "&gt;",
  ['"'] = "&quot;",
  ["'"] = "&#39;",
}
local function escape_html(str)
  return (str:gsub("([&<>\"'])", escape_subs))
end

local function write(t, ...)
  local n = select('#', ...)
  for i = 1, n do
    table.insert(t, (select(i, ...)))
  end
end

local Html = {}

function html.Html(text)
  assert(type(text) == "string", "html.Html expects a string")
  return setmetatable({text = text}, Html)
end

function Html:__tostring()
  return self.text
end

local void_elements = {
  area = true, base = true, br = true, col = true,
  embed = true, hr = true, img = true, input = true,
  link = true, meta = true, param = true, source = true,
  track = true, wbr = true,
}

local function attr_cmp(a, b)
  return a[1] < b[1]
end

local function write_children(el, def)
  for _, child in ipairs(def) do
    if type(child) == "string" then
      table.insert(el, escape_html(child))
    elseif type(child) == "table" and getmetatable(child) == Html then
      table.insert(el, child.text)
    elseif type(child) == "table" then
      write_children(el, child)
    end
  end
end

function html.Element(kind, def)
  if type(def) == "string" then
    def = {def}
  end
  if def == nil then
    def = {}
  end

  local attr = {}
  for k, v in pairs(def) do
    if type(k) == "string" then
      local k = k:gsub("_", "-")
      local v = tostring(v)
      table.insert(attr, {k, v})
    end
  end
  table.sort(attr, attr_cmp)

  -- open tag
  local el = {"<", kind}
  for _, a in ipairs(attr) do
    write(el, " ", a[1], '="', escape_html(a[2]), '"')
  end
  table.insert(el, ">")

  if void_elements[kind] then
    return html.Html(table.concat(el))
  end

  -- children
  write_children(el, def)

  -- close tag
  write(el, "</", kind, ">")

  return html.Html(table.concat(el))
end

function html.Document(def)
  return html.Html("<!doctype html>" .. tostring(html.Element("html", def)))
end

function html.__index(html, key)
  key = key:gsub("_", "-")
  local function thunk(def)
    return html.Element(key, def)
  end
  html[key] = thunk -- memoise for future use; __index will not be called then
  return thunk
end

return html
