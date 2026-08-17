# Coding Guidelines

Coding standards and conventions for Coconut Milk.

## C++ Style

- **C-like, Google-based style** with `struct`s and namespaces
- Avoid heavy class hierarchies unless there is a strong reason
- Use `create(...)` / `destroy(...)` pairs for modules when ownership needs to be explicit
- Keep module state in small structs
- Prefer free functions inside namespaces for behavior
- Keep config as a shared startup object; pass by pointer/reference, not by value

## Naming Conventions

### Files
- `snake_case.h` / `snake_case.cpp` for all source files
- `_fwd.h` for forward-declaration-only headers (e.g. `app_fwd.h`)
- `.mm` for Objective-C++ files (macOS platform layer)

### Types
- `PascalCase` for structs, classes, enums, and usertypes
- `CoconutContext`, `CoconutWindowHandle`, `WebviewTransport`
- Enum values: `VIEW_KIND_FILE`, `VIEW_KIND_HTML`, `VIEW_KIND_URL`

### Functions & Methods
- `camelCase` for free functions and methods
- `snake_case` acceptable for C-style module APIs (`create`, `destroy`)
- Private helpers prefixed with `_` (e.g. `_bindCoconutLuaApi`, `_registerBuiltinCommands`)

### Variables & Constants
- `camelCase` for local and member variables
- `kPascalCase` for constants (e.g. `kQueueCapacity`, `kEvent`)
- `g_` prefix for module-level globals (e.g. `g_dispatch_app`, `g_runloop_source`)

### Namespaces
- `coconut::` top-level, then nested: `coconut::bridge::`, `coconut::dispatch::`, `coconut::lua::`, `coconut::window::`, `coconut::modules::`, `coconut::bg_thread::`

## Module & API Usage

### Module Shape
Each module follows the same basic shape where applicable:
```cpp
T* create(Config* config);
void destroy(T* state);
```
`Config` is created once and shared across modules.

### Platform Adapter Rule
Platform-specific code MUST go through an interface module:
```
DONT: module → platform code directly
DO:   module → interface module → platform code
```
Example: `CoconutWindowHandle::close()` → `webview_terminate()` (via the window interface).
See `src/window.h` and `platform/darwin/window_handle.mm`.

### Thread Safety
- **Main thread**: owns webview, platform APIs, bridge dispatch — must never block
- **Background thread**: runs user commands; owns a separate `lua_State`
- `CoconutContext::is_main_thread` flag determines thread identity
- Thread-safe subset on bg: `coconut.fs.*`, `coconut.json.*`, `coconut.log/info/warn/error`
- Everything else returns a `Future` that forwards to the main thread

## Error Handling

- Prefer `std::expected<T, Error>` or `std::optional` for recoverable failures
- Be defensive; use error-as-value where possible; try/catch where something may fail
- Use `ErrorCode` + `Error` as the shared error vocabulary
- Avoid exceptions for normal control flow
- Avoid silent failure — every error path should log via `debug::error()` / `debug::warn()`

### Error Propagation
- Factory functions return `std::expected<T*, Error>`
- Error chain: `if (!result) { return std::unexpected(result.error()); }`
- Lua-bound functions must catch and wrap errors in a `kError` envelope — never let exceptions propagate to the webview callback

## Testing

### Test Files
- `tests/unit/` — isolated component tests (one per module)
- `tests/integration/` — cross-component flows
- `tests/e2e/` — full app lifecycle

### Test Naming
- `test_<module>_<scenario>()` for individual test functions
- Test binary: `tests` (xmake target)
- Run: `xmake build tests && xmake run tests`

### Coverage Targets (v0.1.1)
- window, webview_transport, hotreload, context, argparse

## Documentation

### When to Document
- Public API functions and usertypes
- Module entry points (`create` / `destroy`)
- Thread-safety guarantees
- Error conditions and return values

### Style
- Keep it minimal and explicit
- Document the "why" in decisions, the "how" in concepts, the "what" in reference
- Diagrams in `wiki/diagrams/` use Pintora (`.pintora` source + rendered `.png`)
- Cross-reference ADRs from implementation comments where relevant
