(function () {
  'use strict';

  /**
   * Simple ping command.
   * @returns {Promise<{message: string}>}
   */
  async function ping() {
    return coconut.call("ping", {});
  }

  /**
   * Greet someone.
   * @param {{name?: string}} params
   * @returns {Promise<{greeting: string}>}
   */
  async function greet(params) {
    return coconut.call("greet", params);
  }

  window.ping = ping;
  window.greet = greet;

  if (typeof module !== 'undefined' && module.exports) {
    module.exports = { ping, greet };
  }
})();
