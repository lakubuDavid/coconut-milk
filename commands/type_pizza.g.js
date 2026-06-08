// Auto-generated command wrappers. Do not edit.
// Uses __coconut_call for Lua command invocation.
// Plain JS with JSDoc — no build step required.
// @ts-check

/**
 * Does nothing.  No params, no meaningful return.
 * @returns {Promise<any>}
 */
export async function noop() {
  return __coconut_call("noop", {});
}

/**
 * @returns {Promise<string>}
 */
export async function ping() {
  return __coconut_call("ping", {});
}

/**
 * Say hello
 * @param {string} name
 * @returns {Promise<string>}
 */
export async function greet(name) {
  return __coconut_call("greet", {name});
}

/**
 * Add two numbers.
 * A multi-line description that spans
 * several lines to test continuation parsing.
 * @param {number} a
 * @param {number} b
 * @returns {Promise<number>}
 */
export async function sum(a, b) {
  return __coconut_call("sum", {a, b});
}

/**
 * Authenticate a user with optional remember flag.
 * @param {string} username
 * @param {string} password
 * @param {boolean} [remember]
 * @returns {Promise<any>}
 */
export async function login(username, password, remember) {
  return __coconut_call("login", {username, password, remember});
}

/**
 * Full-text search with filter and pagination.
 * @param {string} query
 * @param {{fuzzy?: boolean, limit?: number, offset?: number}} [options]
 * @returns {Promise<string[]>}
 */
export async function search(query, options) {
  return __coconut_call("search", {query, options});
}

/**
 * Apply a function to each element.
 * @param {number[]} data
 * @param {(x: number) => string} fn
 * @returns {Promise<string[]>}
 */
export async function transform(data, fn) {
  return __coconut_call("transform", {data, fn});
}

/**
 * Deep-merge two tables with union type for overrides.
 * @param {{name: string, meta?: object}} base
 * @param {{name?: string, meta?: object} | null | undefined} overrides
 * @returns {Promise<{name: string, meta?: object}>}
 */
export async function merge(base, overrides) {
  return __coconut_call("merge", {base, overrides});
}

/**
 * Evaluate an expression with a typed callback.
 * @param {string | number | boolean} expr
 * @param {(val: string | number) => boolean} validate
 * @returns {Promise<{result: string | number | boolean, valid: boolean}>}
 */
export async function evaluate(expr, validate) {
  return __coconut_call("evaluate", {expr, validate});
}

