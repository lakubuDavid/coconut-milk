// Declares the global `coconut` object provided by the Coconut Milk bridge.
// Full type definition is in scripts/coconut.d.ts.
/// <reference path="../scripts/coconut.d.ts" />

/**
@description List contents of a directory
*/
declare function editor_list_dir(payload:{path: string},) : Promise<[coconut.fs.DirEntry[]]>;
/**
@description Read a file's contents (text or image metadata)
*/
declare function editor_read_file(payload:{path: string},) : Promise<[{content?: string; type: string; text_type?: string; path: string; name: string; error?: string}]>;
/**
@description Save content to a file on disk
*/
declare function editor_save_file(payload:{path: string; content: string},) : Promise<[{ok: boolean; error?: string}]>;
/**
@description Show native file/folder open dialog
*/
declare function editor_open_dialog(payload:{title?: string},) : Promise<[{path?: string; is_dir?: boolean; cancelled: boolean; error?: string}]>;
/**
@description Show native save-file dialog
*/
declare function editor_save_dialog(payload:{default_name?: string},) : Promise<[{path?: string; cancelled: boolean; error?: string}]>;
