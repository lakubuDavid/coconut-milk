// Auto-generated command wrappers. Do not edit.
// Uses __coconut_call for Lua command invocation.
// Plain JS with JSDoc — no build step required.
// @ts-check

/**
 * @param {{entries: {expr: string, result: string}[]}} params
 * @returns {Promise<{ok: boolean}>}
 */
export async function calc_save(params) {
  return __coconut_call("calc_save", {params});
}

/**
 * @returns {Promise<{entries: {expr: string, result: string}[]}>}
 */
export async function calc_load() {
  return __coconut_call("calc_load", {});
}

