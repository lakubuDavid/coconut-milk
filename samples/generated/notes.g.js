// Auto-generated command wrappers. Do not edit.
// Uses coconut.call() for Lua command invocation.
// Plain JS with JSDoc — no build step required.
// @ts-check

(function () {
  'use strict';

  /**
   * Load all saved notes from disk.
   * @returns {Promise<string[]>}
   */
  async function notes_list() {
    return coconut.call("notes_list", {});
  }

  /**
   * Save notes to disk.
   * @param {string[]} notes
   * @returns {Promise<any>}
   */
  async function notes_save(notes) {
    return coconut.call("notes_save", notes);
  }

  // Expose to window for non-module scripts
  window.notes_list = notes_list;
  window.notes_save = notes_save;

  if (typeof module !== 'undefined' && module.exports) {
    module.exports = { notes_list, notes_save };
  }
})();
