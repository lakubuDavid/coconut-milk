// Runtime functions used by the generated wrappers.
// These are provided by the Coconut Milk bridge at runtime.

/**
 * Call a Lua command registered via ctx:bind.
 * @param name - Command name (matches the @command tag).
 * @param payload - Parameters forwarded to the Lua handler.
 * @returns Promise resolving with the Lua return value.
 */
declare function __coconut_call<T >(name: string, payload: Record<string, unknown>): Promise<T>;

/**
@description 
*/
declare function calc_save(params:{entries: {expr: string; result: string}[]},) : Promise<[{ok: boolean}]>;
/**
@description 
*/
declare function calc_load() : Promise<[{entries: {expr: string; result: string}[]}]>;
