# patches

## What it is
A monkey-patch applied to the vendored `webview` fork before build, stored at
`patches/webview-on_configure_config.patch`.

## Why we use it
The upstream webview library does not expose a hook for custom URL scheme
configuration before `WKWebView` creation. The patch adds a static callback
(`on_configure_config`) that lets Coconut register the `coconut://` scheme
handler at the right point in the WKWebView lifecycle.

## Key concepts
- Applied in CI and locally via: `cd thirdparty/webview && git apply $PATCH`
- The patch adds a `static std::function<void(WKWebViewConfiguration *)>` hook
  that fires during webview engine configuration
- On macOS this is called **before** `WKWebView` is instantiated; on other
  platforms it is a no-op (scheme handler registered at runtime instead)

## How we use it here
- `platform::installSchemeHandlerHook(root)` stores the app root path so the
  patch callback can resolve `coconut://` URLs to local files
- `platform::finalizeSchemeHandler(wv)` is a no-op on macOS (already done via
  the patch); on Windows/Linux it registers the scheme handler at runtime
- The patch is idempotent — `git apply --check` is used in CI to skip if
  already applied

## Gotchas
- **Fork drift**: if the vendored webview is updated, the patch may need
  rebasing. Always verify with `git apply --check` after updating the submodule
- **Platform-specific**: the patch only affects the macOS (Cocoa) implementation;
  `scheme_handler_win.cpp` / `scheme_handler_linux.cpp` register handlers
  differently at runtime
