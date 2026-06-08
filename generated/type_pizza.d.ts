// Runtime functions used by the generated wrappers.
// These are provided by the Coconut Milk bridge at runtime.

/**
 * Call a Lua command registered via ctx:bind.
 * @param name - Command name (matches the @command tag).
 * @param payload - Parameters forwarded to the Lua handler.
 * @returns Promise resolving with the Lua return value.
 */
declare function __coconut_call<T>(name: string, payload: Record<string, unknown>): Promise<T>;

/**
@description Does nothing.  No params, no meaningful return.
*/
declare function noop() : Promise<[any]>;
/**
@description 
*/
declare function ping() : Promise<[string]>;
/**
@description Say hello
*/
declare function greet(name:string,) : Promise<[string]>;
/**
@description Add two numbers.
A multi-line description that spans
several lines to test continuation parsing.
*/
declare function sum(a:number,b:number,) : Promise<[number]>;
/**
@description Authenticate a user with optional remember flag.
*/
declare function login(username:string,password:string,remember?:boolean,) : Promise<[any]>;
/**
@description Full-text search with filter and pagination.
*/
declare function search(query:string,options?:{fuzzy?: boolean; limit?: number; offset?: number},) : Promise<[string[]]>;
/**
@description Apply a function to each element.
*/
declare function transform(data:number[],fn:(x: number) => string,) : Promise<[string[]]>;
/**
@description Deep-merge two tables with union type for overrides.
*/
declare function merge(base:{name: string; meta?: Record<string, unknown>},overrides:{name?: string; meta?: Record<string, unknown>} | null | undefined,) : Promise<[{name: string; meta?: Record<string, unknown>}]>;
/**
@description Evaluate an expression with a typed callback.
*/
declare function evaluate(expr:string | number | boolean,validate:(val: string | number) => boolean,) : Promise<[{result: string | number | boolean; valid: boolean}]>;
