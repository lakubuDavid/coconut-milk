// Auto-generated command wrappers. Do not edit.
// Uses coconut.call() for Lua command invocation.
// Plain JS with JSDoc — no build step required.
// @ts-check

/**
 * My command description
 * Somthing
 * @param {{name?: string}} arg0
 * @returns {Promise<string>}
 */
export async function hello(arg0) {
  return coconut.call("hello", {arg0});
}

/**
 * My command description
 * @param {string} name
 * @returns {Promise<string>}
 */
export async function hi(name) {
  return coconut.call("hi", {name});
}

