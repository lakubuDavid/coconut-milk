// Auto-generated command wrappers. Do not edit.
// Uses __coconut_call for Lua command invocation.

/**
 * My command description
 * Somthing
 * @param arg0 { name?: string }
 * @returns string
 */
export async function hello(arg0: { name?: string }): Promise<string> {
  return __coconut_call("hello", {arg0});
}

/**
 * My command description
 * @param name string
 * @param ctx {name?: string }
 * @returns string
 */
export async function hi(name: string): Promise<string> {
  return __coconut_call("hi", {name});
}

