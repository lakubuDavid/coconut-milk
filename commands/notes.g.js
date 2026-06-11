// Auto-generated command wrappers. Do not edit.
// Uses coconut.call() for Lua command invocation.
// Plain JS with JSDoc — no build step required.
// @ts-check

/**
 * Load all saved notes from disk.
 * @returns {Promise<string[]>}
 */
export async function notes_list() {
  return coconut.call("notes_list", {});
}

/**
 * Save notes to disk.
 * @param {string[]} notes
 * @returns {Promise<any>}
 */
export async function notes_save(notes) {
  return coconut.call("notes_save", {notes});
}

