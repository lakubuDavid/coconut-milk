// Runtime functions used by the generated wrappers.
// These are provided by the Coconut Milk bridge at runtime.

/**
 * Call a Lua command registered via ctx:bind.
 * @param name - Command name (matches the @command tag).
 * @param payload - Parameters forwarded to the Lua handler.
 * @returns Promise resolving with the Lua return value.
 */
declare function __coconut_call<T = unknown>(name: string, payload: Record<string, unknown>): Promise<T>;

/**
@description My command description
Somthing
*/
declare function hello(arg0:{name?: string},) : Promise<[string]>;
/**
@description My command description
*/
declare function hi(name:string,ctx:{name?: string},) : Promise<[string]>;
