# coconut-vite

Vite plugin for [Coconut Milk](https://github.com/lakubuDavid/coconut-milk) desktop apps.

Rewrites relative asset paths in the build output to use the `coconut://` scheme,
bypassing `file://` CORS restrictions on ES modules.

## Install

```bash
npm install coconut-vite
# or
bun add coconut-vite
```

## Usage

```js
// vite.config.js
import { defineConfig } from 'vite'
import coconut from 'coconut-vite'

export default defineConfig({
  plugins: [coconut()],
  base: './',
})
```

Coconut config stays as a plain `file` view:

```lua
-- coconut.config.lua
views = {
  app = { kind = "file", src = "dist/index.html" },
}
```

## How it works

| Before (Vite default) | After (plugin) |
|---|---|
| `./assets/index-xxx.js` | `coconut://dist/assets/index-xxx.js` |
| `./assets/style-xxx.css` | `coconut://dist/assets/style-xxx.css` |
| `./favicon.svg` | `coconut://dist/favicon.svg` |

The HTML loads via `file://` (no CORS needed for the document). Assets load
through the native `coconut://` scheme handler, which serves them with
`Access-Control-Allow-Origin: *` — so module scripts work without errors.
