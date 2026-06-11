// Declares the global `coconut` object provided by the Coconut Milk bridge.
// Full type definition is in scripts/coconut.d.ts.
/// <reference path="../scripts/coconut.d.ts" />

/**
@description My command description
Somthing
*/
declare function hello(arg0:{name?: string},) : Promise<[string]>;
/**
@description My command description
*/
declare function hi(name:string,ctx:{name?: string},) : Promise<[string]>;
