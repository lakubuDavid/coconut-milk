// Auto-generated command wrappers. Do not edit.
// Uses __coconut_call for Lua command invocation.
// Plain JS with JSDoc — no build step required.
// @ts-check

/**
 * Load all saved notes from disk.
 * @returns {Promise<string[]>}
 */
export async function notes_list() {
  return __coconut_call("notes_list", {});
}

/**
 * Save notes to disk.
 * @param {string[]} notes
 * @returns {Promise<any>}
 */
export async function notes_save(notes) {
  return __coconut_call("notes_save", {notes});
}

