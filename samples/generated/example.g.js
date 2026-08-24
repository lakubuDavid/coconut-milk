// Auto-generated command wrappers. Do not edit.
// Uses coconut.call() for Lua command invocation.
// Plain JS with JSDoc — no build step required.
// @ts-check

(function () {
  'use strict';

  /**
   * @param {{name?: string}} params
   * @returns {Promise<string>}
   */
  async function goodbye(params) {
    return coconut.call("goodbye", params);
  }

  // Expose to window for non-module scripts
  window.goodbye = goodbye;

  if (typeof module !== 'undefined' && module.exports) {
    module.exports = { goodbye };
  }
})();
