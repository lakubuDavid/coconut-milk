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
@description Load all saved notes from disk.
*/
declare function notes_list() : Promise<[string[]]>;
/**
@description Save notes to disk.
*/
declare function notes_save(notes:string[],) : Promise<[any]>;
