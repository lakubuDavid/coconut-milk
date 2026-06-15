// Auto-generated command wrappers. Do not edit.
// Uses coconut.call() for Lua command invocation.
// Plain JS with JSDoc — no build step required.
// @ts-check

/**
 * @returns {Promise<{projects: object}>}
 */
export async function project_list() {
  return coconut.call("project_list", {});
}

/**
 * @param {{name: string, data: string}} params
 * @returns {Promise<{ok: boolean}>}
 */
export async function project_save(params) {
  return coconut.call("project_save", params);
}

/**
 * @param {{name: string}} params
 * @returns {Promise<{ ok: boolean, data: string? }>}
 */
export async function project_load(params) {
  return coconut.call("project_load", params);
}

/**
 * @param {{name: string}} params
 * @returns {Promise<{ok: boolean}>}
 */
export async function project_delete(params) {
  return coconut.call("project_delete", params);
}

