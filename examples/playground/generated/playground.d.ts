// Declares the global `coconut` object provided by the Coconut Milk bridge.
// Full type definition is in scripts/coconut.d.ts.
/// <reference path="../scripts/coconut.d.ts" />

/**
@description playground - test every Coconut Milk feature interactively.
--- These are extra commands beyond the built-in ones (ping, getViews,
fs_read_text, __coconutWindowCtl, clipboard_*, openUrl, notify,
dialog_*, fs_*).
*/
declare function playground_env() : Promise<[any]>;
/**
@description 
*/
declare function playground_json() : Promise<[any]>;
/**
@description 
*/
declare function playground_echo() : Promise<[any]>;
/**
@description 
*/
declare function playground_send_event() : Promise<[any]>;
