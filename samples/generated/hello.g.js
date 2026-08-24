// Auto-generated command wrappers. Do not edit.
// Uses coconut.call() for Lua command invocation.
// Plain JS with JSDoc — no build step required.
// @ts-check

(function () {
  'use strict';

  /**
   * My command description
   * Somthing
   * @param {{name?: string}} arg0
   * @returns {Promise<string>}
   */
  async function hello(arg0) {
    return coconut.call("hello", arg0);
  }

  /**
   * My command description
   * @param {string} name
   * @returns {Promise<string>}
   */
  async function hi(name) {
    return coconut.call("hi", name);
  }

  // Expose to window for non-module scripts
  window.hello = hello;
  window.hi = hi;

  if (typeof module !== 'undefined' && module.exports) {
    module.exports = { hello, hi };
  }
})();
