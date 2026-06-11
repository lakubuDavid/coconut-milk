// Declares the global `coconut` object provided by the Coconut Milk bridge.
// Full type definition is in scripts/coconut.d.ts.
/// <reference path="../scripts/coconut.d.ts" />

/**
@description 
*/
declare function calc_save(params:{entries: {expr: string; result: string}[]},) : Promise<[{ok: boolean}]>;
/**
@description 
*/
declare function calc_load() : Promise<[{entries: {expr: string; result: string}[]}]>;
