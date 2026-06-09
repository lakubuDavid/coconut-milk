---@diagnostic disable: missing-fields
---@meta

--- Coconut Milk global API definitions.
--- This file is the LuaLS / LuaCATS-facing description of the runtime surface.

---@class CoconutViewSpec
---@field kind "url" | "html" | "file"
---@field value string
---@field name? string
---@field meta? table

--- View descriptors returned by View.url/html/load.
---@class CoconutViewDescriptor

--- Declare default props for this view.
---@field defineProps fun(self: CoconutViewDescriptor, props: table): CoconutViewDescriptor

--- Called once when the view is first loaded/created.
---@field on_load fun(self: CoconutViewDescriptor, fn: fun(ctx: CoconutContext)): CoconutViewDescriptor

--- Called when the view becomes the active/visible view.
---@field on_mount fun(self: CoconutViewDescriptor, fn: fun(ctx: CoconutContext)): CoconutViewDescriptor

--- Called when switching away from this view.
---@field on_unmount fun(self: CoconutViewDescriptor, fn: fun(ctx: CoconutContext)): CoconutViewDescriptor

--- Called when the frontend emits an event while this view is active.
---@field on_frontend_event fun(self: CoconutViewDescriptor, name: string, fn: fun(ctx: CoconutContext, payload: table)): CoconutViewDescriptor

---@class CoconutViewModule
---@field url fun(url: string): CoconutViewDescriptor
---@field html fun(html: string): CoconutViewDescriptor
---@field load fun(path: string): CoconutViewDescriptor

---@type CoconutViewModule
View = {}

---@class CoconutWindowSize
---@field w integer
---@field h integer

--- Screen position / offset for window operations.
---@class CoconutPoint
---@field x integer
---@field y integer

---@class CoconutWindow
---@field show fun(self: CoconutWindow, name: string, props?: table)
---@field reload fun(self: CoconutWindow)
---@field close fun(self: CoconutWindow)
---@field minimize fun(self: CoconutWindow)
---@field maximize fun(self: CoconutWindow)
---@field setFullscreen fun(self: CoconutWindow, on: boolean)
---@field toggleFullscreen fun(self: CoconutWindow)
---@field resize fun(self: CoconutWindow, size: CoconutWindowSize)
---@field setMovableByBackground fun(self: CoconutWindow, on: boolean)
---@field setPosition fun(self: CoconutWindow, x: integer, y: integer)
---@field move fun(self: CoconutWindow, offset: CoconutPoint)
---@field getPosition fun(self: CoconutWindow): CoconutPoint

---@class CoconutContext
---@field window CoconutWindow
---@field props? table
---@field setWindowSize fun(self: CoconutContext, size: CoconutWindowSize): CoconutContext
---@field setMinimumWindowSize fun(self: CoconutContext, size: CoconutWindowSize): CoconutContext
---@field setMaximumWindowSize fun(self: CoconutContext, size: CoconutWindowSize): CoconutContext
---@field setMinimumWindowWidth fun(self: CoconutContext, w: integer): CoconutContext
---@field setMinimumWindowHeight fun(self: CoconutContext, h: integer): CoconutContext
---@field setMaximumWindowWidth fun(self: CoconutContext, w: integer): CoconutContext
---@field setMaximumWindowHeight fun(self: CoconutContext, h: integer): CoconutContext
---@field setTitle fun(self: CoconutContext, title: string): CoconutContext
---@field setResizable fun(self: CoconutContext, on: boolean): CoconutContext
---@field setInitialView fun(self: CoconutContext, name: string): CoconutContext
---@field bind fun(self: CoconutContext, name: string, fn: fun(params: table, ctx: CoconutContext))

---@class CoconutDialogResult
---@field confirmed boolean
---@field path string
---@field paths? string[]

---@class CoconutDialogModule
---@field message fun(message: string, title?: string, kind?: "info"|"warn"|"error"|"question"): CoconutDialogResult
---@field open fun(title?: string, multi?: boolean): CoconutDialogResult
---@field save fun(title?: string, defaultName?: string): CoconutDialogResult

---@class CoconutJsonModule
---@field jsonify fun(obj: table): string
---@field parse fun(str: string): table

---@class CoconutModule
---@field views fun(): table<string, CoconutViewDescriptor|fun(): CoconutViewDescriptor>
---@field config fun(ctx: CoconutContext): CoconutContext
---@field emit fun(name: string, payload: table)
---@field events? fun(name: string, payload: table, ctx: CoconutContext)
---@field on_ready? fun(ctx: CoconutContext)
---@field on_close? fun(ctx: CoconutContext)
---@field on_focus? fun()
---@field on_blur? fun()
---@field on_resize fun(ctx: CoconutContext, w: integer, h: integer)
---@field log fun(msg: string)
---@field info fun(msg: string)
---@field warn fun(msg: string)
---@field error fun(msg: string)
---@field dialog CoconutDialogModule
---@field json CoconutJsonModule

---@type CoconutModule
coconut = {
}
