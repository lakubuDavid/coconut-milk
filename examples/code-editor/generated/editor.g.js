// Auto-generated command wrappers. Do not edit.
// Uses coconut.call() for Lua command invocation.
// Plain JS with JSDoc — no build step required.
// @ts-check

(function () {
  'use strict';

  /**
   * List contents of a directory
   * @param {{path: string}} payload
   * @returns {Promise<coconut.fs.DirEntry[]>}
   */
  async function editor_list_dir(payload) {
    return coconut.call("editor_list_dir", payload);
  }

  /**
   * Read a file's contents (text or image metadata)
   * @param {{path: string}} payload
   * @returns {Promise<{content?: string, type: string, text_type?: string, path: string, name: string, error?: string}>}
   */
  async function editor_read_file(payload) {
    return coconut.call("editor_read_file", payload);
  }

  /**
   * Save content to a file on disk
   * @param {{path: string, content: string}} payload
   * @returns {Promise<{ok: boolean, error?: string}>}
   */
  async function editor_save_file(payload) {
    return coconut.call("editor_save_file", payload);
  }

  /**
   * Show native file/folder open dialog
   * @param {{title?: string}} payload
   * @returns {Promise<{path?: string, is_dir?: boolean, cancelled: boolean, error?: string}>}
   */
  async function editor_open_dialog(payload) {
    return coconut.call("editor_open_dialog", payload);
  }

  /**
   * Show native save-file dialog
   * @param {{default_name?: string}} payload
   * @returns {Promise<{path?: string, cancelled: boolean, error?: string}>}
   */
  async function editor_save_dialog(payload) {
    return coconut.call("editor_save_dialog", payload);
  }

  // Expose to window for non-module scripts
  window.editor_list_dir = editor_list_dir;
  window.editor_read_file = editor_read_file;
  window.editor_save_file = editor_save_file;
  window.editor_open_dialog = editor_open_dialog;
  window.editor_save_dialog = editor_save_dialog;

  if (typeof module !== 'undefined' && module.exports) {
    module.exports = { editor_list_dir, editor_read_file, editor_save_file, editor_open_dialog, editor_save_dialog };
  }
})();
