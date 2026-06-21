/**
 * coconut-vite
 *
 * Vite plugin for Coconut Milk desktop apps.  Replaces relative asset
 * paths in the built HTML with `coconut://` scheme URLs so that module
 * scripts and other assets load through the native scheme handler,
 * which provides proper CORS headers.
 *
 * ## The problem
 *
 * When a webview loads `dist/index.html` via `file://` protocol, any
 * `<script type="module" crossorigin>` triggers a CORS check.  The
 * `file://` origin is opaque (`null`), so the browser blocks the
 * module script → blank white screen.
 *
 * ## The fix
 *
 * This plugin rewrites asset URLs in the build output:
 *
 *   ./assets/index-xxx.js  →  coconut://dist/assets/index-xxx.js
 *   ./favicon.svg          →  coconut://dist/favicon.svg
 *
 * The HTML itself still loads via `kind = "file"` (fast, no CORS
 * needed for the document), while JS/CSS/fonts go through the
 * `coconut://` scheme handler which returns `Access-Control-Allow-Origin: *`.
 *
 * ## Usage
 *
 * ```js
 * // vite.config.js
 * import { defineConfig } from 'vite'
 * import coconut from 'coconut-vite'
 *
 * export default defineConfig({
 *   plugins: [coconut()],
 *   base: './',
 * })
 * ```
 *
 * Then in your coconut.config.lua:
 *
 * ```lua
 * views = {
 *   app = { kind = "file", src = "dist/index.html" },
 * }
 * ```
 *
 * @returns {import('vite').Plugin}
 */
export default function coconut() {
  return {
    name: 'coconut',
    enforce: 'post',
    transformIndexHtml(html) {
      return html.replace(
        /(src|href)=(["'])(\.\.?\/)/gi,
        (match, attr, quote) =>
          `${attr}=${quote}coconut://dist/`
      )
    },
  }
}
