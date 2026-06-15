// Declares the global `coconut` object provided by the Coconut Milk bridge.
// Full type definition is in scripts/coconut.d.ts.
/// <reference path="../scripts/coconut.d.ts" />

/**
@description Load all saved notes from disk.
*/
declare function notes_list() : Promise<[string[]]>;
/**
@description Save notes to disk.
*/
declare function notes_save(notes:string[],) : Promise<[any]>;
