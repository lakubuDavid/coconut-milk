// Auto-generated command wrappers. Do not edit.
// Uses coconut.call() for Lua command invocation.
// Plain JS with JSDoc — no build step required.
// @ts-check

/**
 * @param {{name?: string}} params
 * @returns {Promise<string>}
 */
export async function hello(params) {
  return coconut.call("hello", {params});
}

/**
 * @param {{name?: string}} params
 * @returns {Promise<string>}
 */
export async function goodbye(params) {
  return coconut.call("goodbye", {params});
}

