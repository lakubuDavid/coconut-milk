# Getting Started with Coconut Milk

## Install

### Prerequisites

- C++20 toolchain (Clang 16+ or GCC 13+)
- [xmake](https://xmake.io) — build system
- [Bun](https://bun.sh) — for bridge JS bundling
- Python 3 — for embed header generation

### Build from source

```bash
git clone https://github.com/lakubuDavid/coconut-milk.git
cd coconut-milk
xmake build coconut-milk
```

### Install to $HOME/tools

```bash
just install
```

This symlinks `coconut-milk` and `create-coconut-app` into `$HOME/tools/`.

### Create your first app

```bash
create-coconut-app my-app --template bare
cd my-app
xmake run coconut-milk
```

---

## Project layout

A minimal Coconut Milk app looks like this:

```
my-app/
├── main.lua              # Entry point
├── coconut.config.lua    # Optional config file
├── views/                # HTML view files
│   └── home.html
├── commands/             # Lua command modules
│   └── hello.lua
└── assets/               # Static files (CSS, JS, images)
    └── style.css
```

### `main.lua`

The entry point exposes three functions:

```lua
function coconut.views()
  return {
    home = View.load("views/home.html"),
  }
end

function coconut.config(ctx)
  return ctx
    :setWindowSize({ w = 1280, h = 640 })
    :setInitialView("home")
end

function coconut.commands(ctx)
  -- Register commands here
end
```

### `coconut.config.lua` (optional)

```lua
return {
  window_width = 1280,
  window_height = 640,
  initial_view = "home",
  view_root = "views",
  asset_root = "assets",
  command_root = "commands",
}
```

---

## Templates

### Bare template

```bash
create-coconut-app my-app --template bare
```

A minimal HTML + CSS + JS app. No build step.

### Bare TypeScript template

```bash
create-coconut-app my-app --template bare-ts
```

Same as bare but with TypeScript and `tsc` for compilation.

### Vite template

```bash
create-coconut-app my-app --template vite --framework vue
# or --framework react
# or --framework solid
```

Scaffolds a Vite project with the chosen framework. The `vite.config.js` is pre-configured with `base: './'` for `file://` compatibility.

```bash
cd my-app
bun install
bun run build        # produces dist/
# then from the Coconut Milk root:
xmake run coconut-milk --root my-app
```

---

## Development workflow

### 1. Edit your views

Views are plain HTML files (or Vue/React apps). Coconut injects the `coconut` global automatically.

```html
<!-- views/home.html -->
<!DOCTYPE html>
<html>
<head>
  <link rel="stylesheet" href="coconut://assets/style.css">
</head>
<body>
  <h1>Hello from Coconut Milk</h1>
  <script>
    (async () => {
      await coconut.ready();
      const names = await coconut.call('hello', { name: 'World' });
      document.querySelector('h1').textContent = names;
    })();
  </script>
</body>
</html>
```

### 2. Add commands

```lua
-- commands/hello.lua
---@command hello
---@param params { name?: string }
---@return string
local function hello(params, ctx)
  return "Hello, " .. (params.name or "user") .. "!"
end

return { hello = hello }
```

### 3. Generate typed wrappers

```bash
./bin/coconut-milk-generators commands/hello.lua --out-dir generated
```

This produces:
- `generated/hello.g.lua` — Lua registration glue
- `generated/hello.d.ts` — TypeScript declarations
- `generated/hello.g.js` — JavaScript wrappers

### 4. Run

```bash
xmake run coconut-milk
```

---

## View lifecycle callbacks

Views created with `coconut.views()` support lifecycle callbacks:

```lua
function coconut.views()
  return {
    home = View.load("views/home.html")
      :on_load(function(ctx)
        print("Home loaded once")
      end)
      :on_mount(function(ctx)
        print("Home visible")
      end)
      :on_unmount(function(ctx)
        print("Home hidden")
      end),
  }
end
```

| Callback | When |
|---|---|
| `on_load` | Once when the view is first created |
| `on_mount` | Every time the view becomes visible |
| `on_unmount` | Every time the view is hidden |

---

## Frameless and transparent windows

```lua
function coconut.config(ctx)
  ctx
    :setWindowSize({ w = 480, h = 700 })
    :setFrameless(true)
    :setTransparent(true)
    :setTitle("My App")
  return ctx
end
```

When `transparent` is true, the frontend receives a `transparent-window` CSS class on `<body>` so you can style accordingly.

---

## CLI options

```
coconut-milk [options]
  -h, --help       Show help
  -v, --version    Show version
  -d, --debug      Enable debug mode
  -r, --root PATH  Set project root directory
```

---

## Next steps

- Read the [API Reference](api-reference.md)
- Learn about [Command Generation](generators.md)
- Check the [Specification](specs.md)
