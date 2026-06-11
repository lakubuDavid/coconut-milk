// Auto-generated command wrappers. Do not edit.
// Uses coconut.call() for Lua command invocation.
// Plain JS with JSDoc — no build step required.
// @ts-check

/**
 * @param {{entries: {expr: string, result: string}[]}} params
 * @returns {Promise<{ok: boolean}>}
 */
export async function calc_save(params) {
  return coconut.call("calc_save", {params});
}

/**
 * @returns {Promise<{entries: {expr: string, result: string}[]}>}
 */
export async function calc_load() {
  return coconut.call("calc_load", {});
}

/**
 * @param {{theme: string, precision: number, sound: boolean}} params
 * @returns {Promise<{ok: boolean}>}
 */
export async function settings_save(params) {
  return coconut.call("settings_save", {params});
}

/**
 * @returns {Promise<{theme?: string, precision?: number, sound?: boolean}>}
 */
export async function settings_load() {
  return coconut.call("settings_load", {});
}

