---@diagnostic disable: missing-fields
---@meta

--- Coconut Milk global API definitions.
--- This file is the LuaLS / LuaCATS-facing description of the runtime surface.

---@class CoconutViewSpec
---@field kind "url" | "html" | "file"
---@field value string
---@field name? string
---@field meta? table

---@class CoconutWindowSize
---@field w integer
---@field h integer

---@class CoconutContext
---@field setBrowser fun(self: CoconutContext, mode: string): CoconutContext
---@field setWindowSize fun(self: CoconutContext, size: CoconutWindowSize): CoconutContext
---@field setInitialView fun(self: CoconutContext, name: string): CoconutContext
---@field show fun(self: CoconutContext, name: string)
---@field reload fun(self: CoconutContext)
---@field close fun(self: CoconutContext)
---@field bind fun(self: CoconutContext, name: string, fn: fun(params: table, ctx: CoconutContext))
---@field emit fun(self: CoconutContext, name: string, payload: table)
---@field emit_sync fun(self: CoconutContext, name: string, payload: table)

---@class CoconutModule
---@field views fun(): table<string, CoconutViewSpec>
---@field config fun(ctx: CoconutContext): CoconutContext
---@field on_ready? fun(ctx: CoconutContext)
---@field on_close? fun(ctx: CoconutContext)
---@field on_focus? fun()
---@field on_blur? fun()
---@field on_resize fun(w: integer, h: integer)

---@type CoconutModule
coconut = {}
