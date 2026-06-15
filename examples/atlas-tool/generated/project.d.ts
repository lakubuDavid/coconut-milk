// Declares the global `coconut` object provided by the Coconut Milk bridge.
// Full type definition is in scripts/coconut.d.ts.
/// <reference path="../scripts/coconut.d.ts" />

/**
@description 
*/
declare function project_list() : Promise<[{projects: Record<string, unknown>}]>;
/**
@description 
*/
declare function project_save(params:{name: string; data: string},) : Promise<[{ok: boolean}]>;
/**
@description 
*/
declare function project_load(params:{name: string},) : Promise<[{ ok: boolean, data: string? }]>;
/**
@description 
*/
declare function project_delete(params:{name: string},) : Promise<[{ok: boolean}]>;
