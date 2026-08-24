#ifndef COCONUT_MODULES_REGISTRY_H
#define COCONUT_MODULES_REGISTRY_H

#include <cstdint>

namespace coconut {
  namespace modules {

    /// Bitmask registry of all Lua-facing module bindings.
    ///
    /// Each flag corresponds to one `init_<module>(sol::state&, ThreadKind)`
    /// function in this directory.  Flags are powers of two so they can be
    /// combined with bitwise OR for batch operations (e.g. "enable all
    /// thread-safe modules on the background thread").
    enum class ModulesFlag : uint16_t {
      None = 0,

      /// Core serialisation / logging — safe on any thread.
      kJson = 1 << 0,
      kLog  = 1 << 1,

      /// File system — safe on any thread.
      kFs = 1 << 2,

      /// Environment variables — safe on any thread.
      kEnv = 1 << 3,

      /// Key-value store — safe on any thread.
      kStore = 1 << 4,

      /// Open URL in system browser — safe on any thread.
      kOpenUrl = 1 << 5,

      /// Keyboard shortcut handling — main-thread only.
      kKeybind = 1 << 6,

      /// Native file dialogs — main-thread only.
      kDialog = 1 << 7,

      /// System notifications — main-thread only.
      kNotify = 1 << 8,

      /// Clipboard read/write — main-thread only.
      kClipboard = 1 << 9,

      /// Hot module reload (coconut.hotreload) — main-thread only.
      kHotreload = 1 << 10,

      /// Bridge event emission (coconut._emit_to_js) — main-thread only.
      kBridgeEmit = 1 << 11,

      /// Main-thread stubs (placeholder registrations for mt-only APIs).
      kStubs = 1 << 12,

      /// Background-thread stubs (warnings for mt-only APIs on bg).
      kBgStubs = 1 << 13,

      /// Live window mutations (main: direct; bg: marshalled via dispatch::post).
      kWindow = 1 << 14,

      /// Convenience: all thread-safe modules (bg-thread safe subset).
      ThreadSafe = kJson | kLog | kFs | kEnv | kStore | kOpenUrl,

      /// Convenience: all main-thread-only modules.
      MainOnly =
          kKeybind | kDialog | kNotify | kClipboard | kHotreload | kBridgeEmit | kStubs | kBgStubs,

      /// Convenience: every module flag combined.
      ALL = ThreadSafe | MainOnly,
    };

    /// Bitwise OR for ModulesFlag.
    inline constexpr ModulesFlag operator|(ModulesFlag a, ModulesFlag b) {
      return static_cast<ModulesFlag>(static_cast<uint16_t>(a) | static_cast<uint16_t>(b));
    }

    /// Bitwise AND for ModulesFlag.
    inline constexpr ModulesFlag operator&(ModulesFlag a, ModulesFlag b) {
      return static_cast<ModulesFlag>(static_cast<uint16_t>(a) & static_cast<uint16_t>(b));
    }

    /// Bitwise NOT for ModulesFlag.
    inline constexpr ModulesFlag operator~(ModulesFlag a) {
      return static_cast<ModulesFlag>(~static_cast<uint16_t>(a));
    }

    /// Check if `flag` is set in `mask`.
    inline constexpr bool has(ModulesFlag mask, ModulesFlag flag) {
      return (static_cast<uint16_t>(mask & flag)) != 0;
    }

  }  // namespace modules
}  // namespace coconut

#endif  // COCONUT_MODULES_REGISTRY_H
