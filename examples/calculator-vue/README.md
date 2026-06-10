# Calculator Vue + Coconut

Vue 3 + Vite 8 calculator with persistent history saved through Lua.

## Stack

- **Frontend**: Vue 3 + Vite 8 (Rolldown)
- **Backend**: Coconut Milk (Lua commands → C++ → native webview)
- **Dev server**: `bun run dev` → `http://localhost:5173`
- **JS bindings**: Auto-generated from `@command` annotations in Lua

## Usage

### Dev mode (hot reload)

```bash
# Terminal 1 — start Vite dev server
bun run dev

# Terminal 2 — run coconut with env flag
COCONUT_DEV=1 ../../bin/coconut-milk
```

### Production mode

```bash
bun run build
../../bin/coconut-milk
```

`os.getenv("COCONUT_DEV")` switches the view:
- `COCONUT_DEV=1` → `View.url("http://localhost:5173")`
- unset → `View.load("dist/index.html")` (built files, no server needed)

## Project structure

```
calculator-vue/
├── coconut.config.lua    # env-based URL switching
├── main.lua              # entry point
├── commands/
│   ├── calc.lua          # Lua module with @command annotations
│   ├── calc.g.lua        # generated registration
│   ├── calc.g.js         # generated JS bindings
│   └── calc.d.ts         # generated TS types
├── src/
│   ├── App.vue           # calculator component
│   ├── calc.g.js         # generated bindings (copied)
│   ├── main.js           # Vue entry
│   └── style.css
├── dist/                 # Vite build output
├── index.html            # Vite dev entry
├── vite.config.js
└── package.json
```

## Missing Coconut features

| Feature | Status |
|---------|--------|
| External link handling | ❌ WKNavigationDelegate broken (white screen) |
| Hot reload for file views | ❌ No file watcher |
| Debug/devtools | ❌ `debug` field not wired |
| Windows/Linux URL scheme | ❌ Stubs only |
| Native menus | ❌ Missing |
| Window state persistence | ❌ Missing |
| System tray / dock | ❌ Missing |
| Drag & drop files | ❌ Missing |
| Full frameless + transparent | ⚠️ Needs CSS classes |
| Generated JS bindings | ✅ Works |
| Lua → JS events | ✅ Works |
| Filesystem / JSON | ✅ Works |
