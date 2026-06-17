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
--- Lifecycle event object — passed to view:on_load, :on_mount, :on_unmount, etc.
--- The callback receives an event object with ctx and props fields instead of
--- a bare context table:
---   view:on_mount(function(e) e.ctx:set_title("Hi") end)
---@class CoconutViewLifecycleEvent
---@field name string
---@field target string
---@field type string
---@field ctx CoconutContext
---@field props table

---@class CoconutViewDescriptor

--- Declare default props for this view.
---@field defineProps fun(self: CoconutViewDescriptor, props: table): CoconutViewDescriptor

--- Called once when the view is first loaded/created.
--- Receives a lifecycle event with ctx + props.
---@field on_load fun(self: CoconutViewDescriptor, fn: fun(e: CoconutViewLifecycleEvent)): CoconutViewDescriptor

--- Called when the view becomes the active/visible view.
--- Receives a lifecycle event with ctx + props.
---@field on_mount fun(self: CoconutViewDescriptor, fn: fun(e: CoconutViewLifecycleEvent)): CoconutViewDescriptor

--- Called when switching away from this view.
--- Receives a lifecycle event with ctx + props.
---@field on_unmount fun(self: CoconutViewDescriptor, fn: fun(e: CoconutViewLifecycleEvent)): CoconutViewDescriptor

--- Called before the window closes while this view is active.
--- Call e:preventDefault() to veto the close.
---@field on_before_close fun(self: CoconutViewDescriptor, fn: fun(e: table)): CoconutViewDescriptor

--- Called when an event fires while this view is active.
--- Fires before global coconut.on() listeners.
---@field on fun(self: CoconutViewDescriptor, name: string, fn: fun(event: table)): CoconutViewDescriptor

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
---@field setBackgroundColor fun(self: CoconutWindow, r: number, g: number, b: number, a?: number)

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
---@field setFrameless fun(self: CoconutContext, on: boolean): CoconutContext
---@field setTransparent fun(self: CoconutContext, on: boolean): CoconutContext
---@field setInitialView fun(self: CoconutContext, name: string): CoconutContext
---@field bind fun(self: CoconutContext, name: string, fn: fun(params: table, ctx: CoconutContext))

---@class CoconutDialogResult
---@field confirmed boolean
---@field path string
---@field paths? string[]

---@class CoconutDialogModule
---@field message fun(message: string, title?: string, kind?: "info"|"warn"|"error"|"question"): CoconutDialogResult
---@field open fun(title?: string, multi?: boolean, chooseDir?: boolean): CoconutDialogResult
---@field save fun(title?: string, defaultName?: string): CoconutDialogResult

---@class CoconutJsonModule
---@field jsonify fun(obj: table): string
---@field parse fun(str: string): table

---@class CoconutFsModule
---@field readText fun(path: string): string
---@field readBytes fun(path: string): string
---@field writeText fun(path: string, content: string): boolean
---@field writeBytes fun(path: string, data: string): boolean
---@field exists fun(path: string): boolean
---@field resolve fun(root: string, relpath: string): string
---@field listDir fun(path: string): table

--- Environment variable access (uses __index metamethod).
--- Any string key looks up the env var via getenv().
--- Predefined keys: cwd, homedir, pathSeparator
---@class CoconutEnvModule
---@field cwd string
---@field homedir string
---@field pathSeparator string
---@field [string] string|nil

---@class CoconutClipboardModule
---@field readText fun(): string
---@field writeText fun(text: string): boolean

---@class CoconutModule
---@field views fun(): table<string, CoconutViewDescriptor|fun(): CoconutViewDescriptor>
---@field config fun(ctx: CoconutContext): CoconutContext
---@field emit fun(event: table)
---@field events? fun(event: table)
---@field on fun(name: string, fn: fun(event: table), opts?: {once?: boolean}): function
---@field log fun(msg: string)
---@field info fun(msg: string)
---@field warn fun(msg: string)
---@field error fun(msg: string)
---@field openUrl fun(url: string): boolean
---@field notify fun(title: string, body: string): boolean
---@field dialog CoconutDialogModule
---@field json CoconutJsonModule
---@field fs CoconutFsModule
---@field env CoconutEnvModule
---@field clipboard CoconutClipboardModule

---@type CoconutModule
coconut = {
}
