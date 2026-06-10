# Calculator Vue + Coconut

A Vue calculator app with persistent history saved through Lua to a JSON file.

## Project structure

```
calculator-vue/
├── coconut.config.lua    # Coconut project config
├── main.lua              # Entry point (env-aware URL switching)
├── commands/
│   ├── calc.lua          # Lua command module (@command annotations)
│   ├── calc.g.lua        # Generated command registration
│   └── calc.g.js         # Generated JS bindings
├── views/
│   └── app.html          # Production HTML shell
├── src/
│   ├── main.js           # Vue entry
│   ├── App.vue           # Calculator component
│   ├── calc.g.js         # Generated JS bindings (copied from commands/)
│   ├── assets/
│   └── style.css
├── index.html            # Vite dev server entry
├── vite.config.js
└── package.json
```

## Usage

### Dev mode (Vite dev server + hot reload)

```bash
# Terminal 1: start Vite dev server
bun run dev

# Terminal 2: run coconut with dev flag
COCONUT_DEV=1 ../../bin/coconut-milk
```

### Production mode (built output)

```bash
# Build the Vue app
bun run build

# Run coconut pointing to built files
../../bin/coconut-milk
```

### How env switching works

`coconut.config.lua` checks `os.getenv("COCONUT_DEV")`:
- **`COCONUT_DEV=1`** → loads from `http://localhost:5173` (Vite dev server)
- **unset** → loads `views/app.html` which references the built `coconut://dist/assets/index.js`

## Missing Coconut features

| Feature | Status | Notes |
|---------|--------|-------|
| **External link handling** | ❌ Broken | WKNavigationDelegate causes white screen; need JS-level interception instead |
| **Hot reload for file views** | ❌ Missing | No file watcher; must restart app to see HTML/CSS changes |
| **Debug/devtools** | ❌ Missing | `debug` field in Config not wired to `webview_create` |
| **Windows/Linux URL scheme** | ❌ Stubs only | `coconut://` only works on macOS |
| **Native menus** | ❌ Missing | No macOS menu bar integration |
| **Window state persistence** | ❌ Missing | Position/size not saved between runs |
| **Multiple windows** | ❌ Not planned | Single-window design |
| **System tray / dock** | ❌ Missing | No background/notification support |
| **Drag & drop files** | ❌ Missing | No file drop events |
| **Full frameless + transparent** | ⚠️ Partial | CSS needs `transparent-window` class on `<body>` |
| **Generated JS bindings** | ✅ Works | `calc.g.js` auto-generated from `@command` annotations |
| **Lua → JS events** | ✅ Works | `coconut.emit()` / `coconut.on()` |
| **Filesystem access** | ✅ Works | `coconut.fs.readText/writeText/exists` |
| **JSON parsing** | ✅ Works | `coconut.json.parse/jsonify` |
