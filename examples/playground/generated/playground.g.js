// Auto-generated command wrappers. Do not edit.
// Uses coconut.call() for Lua command invocation.
// Plain JS with JSDoc — no build step required.
// @ts-check

(function () {
  'use strict';

  /**
   * playground - test every Coconut Milk feature interactively.
   * --- These are extra commands beyond the built-in ones (ping, getViews,
   * fs_read_text, __coconutWindowCtl, clipboard_*, openUrl, notify,
   * dialog_*, fs_*).
   * @returns {Promise<any>}
   */
  async function playground_env() {
    return coconut.call("playground_env", {});
  }

  /**
   * @returns {Promise<any>}
   */
  async function playground_json() {
    return coconut.call("playground_json", {});
  }

  /**
   * @returns {Promise<any>}
   */
  async function playground_echo() {
    return coconut.call("playground_echo", {});
  }

  /**
   * @returns {Promise<any>}
   */
  async function playground_send_event() {
    return coconut.call("playground_send_event", {});
  }

  // Expose to window for non-module scripts
  window.playground_env = playground_env;
  window.playground_json = playground_json;
  window.playground_echo = playground_echo;
  window.playground_send_event = playground_send_event;

  if (typeof module !== 'undefined' && module.exports) {
    module.exports = { playground_env, playground_json, playground_echo, playground_send_event };
  }
})();
