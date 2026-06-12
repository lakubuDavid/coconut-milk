# CLI Reference

Coconut Milk provides two command-line tools:

- **`coconut`** — The main runtime binary (run apps, generate code)
- **`create-coconut-app`** — Project scaffolding script

---

## coconut Binary

The `coconut` binary is the core runtime. It has two modes: **app runner** (default) and **generator** (subcommand).

### Usage

```
coconut [options]              # Run the app (default mode)
coconut generate [options]     # Run the code generator
coconut --help                 # Show help (app mode)
coconut generate --help        # Show help (generator mode)
```

### App Runner Mode (Default)

When run without a subcommand, `coconut` starts the desktop application.

#### How It Works

1. **Determine project root** — Uses the current working directory
2. **Load config** — Parses `coconut.config.lua` (if present)
3. **Load Lua entry** — Runs `main.lua` from the project root
4. **Execute callbacks** — `coconut.config(ctx)`, `coconut.views()`, `coconut.commands(ctx)`
5. **Auto-load commands** — Scans `commands/` folder for `*.lua` files
6. **Create webview** — Opens the initial view in a native window
7. **Run event loop** — Dispatches RPC messages between frontend and Lua

#### Options

| Flag | Long | Description |
|---|---|---|
| `-h` | `--help` | Show help message and exit |
| `-v` | `--version` | Show version number and exit |
| `-d` | `--debug` | Enable debug mode (verbose logging) |
| `-r` | `--root PATH` | Set project root directory (overrides CWD) |

#### Examples

```bash
# Run from current directory
coconut

# Run with debug logging
coconut --debug

# Run from a specific directory
coconut --root /path/to/my-app

# Show version
coconut --version
# Output: 0.1.0
```

#### Debug Mode

With `--debug`, additional diagnostic information is logged:

- Window hierarchy dumps
- Bridge message details
- View loading steps
- Command registration

You can also control logging via `coconut.config.lua`:

```lua
return {
  debug = {
    enabled = true,
    showTransportDump = true,  -- Dump all RPC messages
    logLevel = "debug",         -- "debug", "info", "warn", "error"
  },
}
```

---

### coconut generate

The `generate` subcommand runs the code generator. It scans `commands/*.lua` files for `@command` annotations and generates typed wrappers.

#### Usage

```
coconut generate [options]
```

#### Options

| Flag | Long | Description |
|---|---|---|
| `--out-dir DIR` | — | Output directory for generated files (default: from config) |
| `-h` | `--help` | Show help message and exit |

#### How It Works

1. **Load config** — Reads `coconut.config.lua` for `command_root` and `output_dir`
2. **Scan command files** — Finds all `*.lua` files in `command_root` (default: `commands/`)
3. **Parse annotations** — Extracts `@command`, `@param`, `@return` from each file
4. **Generate files** — Writes `.g.lua`, `.d.ts`, `.g.js` per command
5. **Aggregate types** — Writes `generated/commands.d.ts` with union type of all command names
6. **Generate coconut.d.ts** — Writes main type definition at project root

#### Generated Files

For a command file `commands/greet.lua`:

```lua
---@command greet
---@param params { name?: string }
---@return { greeting: string }
local function greet(params, ctx)
  return { greeting = "Hello, " .. (params.name or "World") .. "!" }
end

return { greet = greet }
```

The generator produces:

```
generated/
├── greet.d.ts         # TypeScript types
├── greet.g.js         # JavaScript wrapper
├── greet.g.lua        # Lua registration glue
└── commands.d.ts      # Aggregated: type CoconutCommandName = "greet" | ...

coconut.d.ts           # Main type definitions (project root)
```

**greet.d.ts:**

```typescript
export type GreetParams = { name?: string }
export type GreetResult = { greeting: string }
export declare function greet(payload: GreetParams): Promise<GreetResult>
export default greet
```

**greet.g.js:**

```js
// Auto-generated. Do not edit.
export async function greet(payload) {
  return coconut.call("greet", payload)
}
```

**greet.g.lua:**

```lua
-- Auto-generated. Do not edit.
local impl = require("commands.greet")

local function register(ctx)
  ctx:bind("greet", impl.greet)
  return ctx
end

return register
```

**commands.d.ts:**

```typescript
export type CoconutCommandName = "greet" | "farewell" | "ping";
```

#### Output Directory

The output directory is determined by:

1. `--out-dir` flag (highest priority)
2. `generators.output_dir` in `coconut.config.lua`
3. `output_dir` in `coconut.config.lua`
4. Default: `generated`

```lua
-- coconut.config.lua
return {
  generators = {
    output_dir = "generated"  -- Or use top-level: output_dir = "generated"
  },
}
```

#### Examples

```bash
# Generate with defaults (reads config)
coconut generate

# Generate to a specific directory
coconut generate --out-dir build/generated

# Generate from a specific project root
coconut --root /path/to/my-app generate
```

---

## create-coconut-app

The `create-coconut-app` script scaffolds a new Coconut Milk project. It's a Lua script (runs with LuaJIT) that generates project files from templates.

### Usage

```
create-coconut-app [project-name] [options]
```

### Options

| Flag | Long | Description | Default |
|---|---|---|---|
| `-t` | `--template TYPE` | Template type: `bare`, `bare-ts`, `vite` | `bare` |
| `-f` | `--framework FW` | Framework for vite: `vanilla`, `react`, `vue`, `solid` | `vanilla` |
| `-p` | `--pm MANAGER` | Package manager for vite: `npm`, `bun` | `npm` |
| `-y` | `--yes` | Skip prompts, use defaults | `false` |
| `-h` | `--help` | Show help | — |

### Templates

#### bare

Minimal HTML + CSS + JS. No build step, no dependencies.

```bash
create-coconut-app my-app --template bare
```

**Generated files:**

```
my-app/
├── main.lua
├── coconut.config.lua
├── coconut.d.ts
├── tsconfig.json
├── commands/
│   ├── example.lua
│   └── example.g.js
└── views/
    ├── index.html
    ├── style.css
    └── app.js
```

#### bare-ts

Same as bare, but with proper TypeScript support.

```bash
create-coconut-app my-app --template bare-ts
```

**Additional files:**

```
my-app/
├── src/
│   └── app.ts          # TypeScript source
└── tsconfig.json       # Configured with outDir: "dist"
```

**Running:**

```bash
cd my-app
tsc                    # Compile TypeScript
coconut                # Run the app
```

#### vite

Full Vite + framework setup with hot reload.

```bash
create-coconut-app my-app --template vite --framework vue
```

**Generated files:**

```
my-app/
├── main.lua              # Dev/prod view switching
├── coconut.config.lua    # Dev/prod config
├── package.json          # Vite + framework deps
├── vite.config.js        # Pre-configured with base: './'
├── index.html            # Vite entry
├── src/
│   ├── main.js           # Framework entry
│   └── App.vue           # Component
└── commands/
    ├── example.lua
    └── example.g.js
```

**Running in dev mode:**

```bash
# Terminal 1: Start Vite
npm run dev

# Terminal 2: Run Coconut
COCONUT_DEV=1 coconut
```

### Interactive Mode

If you omit flags, the CLI prompts you:

```bash
create-coconut-app
Project name: [my-coconut-app] my-app
Template:
  1) bare (default)
  2) bare-ts
  3) vite
Choice [bare]: 1

Done! Created project in ./my-app

To get started:
  cd my-app
  /path/to/coconut
```

### Non-Interactive Mode

Use `-y` to skip prompts:

```bash
create-coconut-app my-app -y
# Uses defaults: template=bare, framework=vanilla, pm=npm
```

### Overwrite Behavior

If the target directory already exists:

```bash
create-coconut-app existing-app
Directory './existing-app' exists. Overwrite? [y/N]: y
# Deletes and recreates the directory
```

With `-y`, overwrite is automatic:

```bash
create-coconut-app existing-app -y
# Overwrites without asking
```

---

## Justfile Recipes

The project includes a `Justfile` with common development tasks.

### Installation

```bash
just install
# Symlinks coconut and create-coconut-app to ~/tools/
```

### Running Examples

```bash
just run-editor       # Code editor example (CodeMirror 6)
just run-ocr          # OCR scanner example (Alpine.js + Tesseract)
just run-lua-html     # Lua HTML DSL example
just run-vue          # Calculator Vue example
```

### Building

```bash
just build            # Build the coconut binary
just test             # Build and run tests
just clean            # Clean build artifacts
just rebuild          # Clean + build
just format           # Run clang-format on source files
```

### Debug Mode

```bash
just debug            # Configure debug mode, build, and run
```

---

## Next Steps

- Read the **[Getting Started](./getting-started.md)** guide for installation and setup
- Check the **[API Reference](./api-reference.md)** for all functions
- See **[Examples](./examples.md)** for real-world projects
