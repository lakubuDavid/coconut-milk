// Declares the global `coconut` object provided by the Coconut Milk bridge.
// Full type definition is in scripts/coconut.d.ts.
/// <reference path="../scripts/coconut.d.ts" />

/**
@description 
*/
declare function hello(params:{name?: string},) : Promise<[string]>;
/**
@description 
*/
declare function goodbye(params:{name?: string},) : Promise<[string]>;
