type CoconutPayload = Record<string, unknown>

export type CoconutError = {
  code: string
  message: string
  details?: unknown
}

type CoconutEventCallback<TPayload extends CoconutPayload = CoconutPayload> = (
  payload: TPayload,
) => void

type CoconutCallWireEnvelope =
  | { ok: true; data: unknown }
  | { ok: false; error: CoconutError }

type Unsubscribe = () => void

/**
 * Bound by C++.
 * Used by `coconut.call(...)`.
 *
 * @returns A JSON string envelope: `{ ok:true,data }` or `{ ok:false,error }`.
 */
declare function __coconut_call(name: string, payloadJson: string): Promise<string>

/**
 * Bound by C++.
 * Used by `coconut.emit(...)`.
 *
 * Success may return empty/undefined. If it returns JSON, it must be an error envelope.
 */
declare function __coconut_emit(name: string, payloadJson: string): Promise<string | undefined>

const _listeners = new Map<string, Set<CoconutEventCallback>>()

let _ready = false
let _readyResolve: (() => void) | undefined
const _readyPromise = new Promise<void>((resolve) => {
  _readyResolve = resolve
})

function _markReady() {
  if (_ready) return
  _ready = true
  _readyResolve?.()
}

/**
 * Called by injected JS from C++ once the Coconut bridge is active.
 *
 * This resolves `coconut.ready()`.
 */
;(globalThis as any).__coconut_bridge_ready = () => {
  _markReady()
}

/**
 * Called by the native transport to deliver kReturn/kError RPC responses.
 * Currently a no-op; will be used for promise resolution when all traffic
 * routes through the transport (post-webview migration).
 */
;(globalThis as any).__coconut_rpc_receive = (_msgJson: string): void => {
  // TODO: resolve/reject pending promise map
}

/**
 * Called by injected JS from C++ to deliver Lua -> JS events.
 *
 * @param name event name
 * @param payloadJson JSON-string serialized payload
 */
;(globalThis as any).__coconut_dispatch_event = (name: string, payloadJson: string) => {
  const set = _listeners.get(name)
  if (!set || set.size === 0) return

  let payload: CoconutPayload = {}
  if (payloadJson && payloadJson.length > 0) {
    try {
      payload = JSON.parse(payloadJson)
    } catch {
      payload = {}
    }
  }

  for (const cb of Array.from(set)) {
    try {
      cb(payload)
    } catch {
      // Listener errors should not break dispatch.
    }
  }
}

function _stringifyPayload(payload: CoconutPayload): string {
  return JSON.stringify(payload ?? {})
}

/**
 * Coconut frontend bridge API.
 *
 * Communication:
 * - payloads cross the bridge as JSON strings
 * - events are delivered via injected dispatcher callbacks
 */
const coconut = {

  ready: async () => {
    if (_ready) return
    await _readyPromise
  },

  on: (event: string, callbackFn: CoconutEventCallback) : Unsubscribe => {
    let set = _listeners.get(event)
    if (!set) {
      set = new Set()
      _listeners.set(event, set)
    }
    set.add(callbackFn)

    return () => {
      set?.delete(callbackFn)
    }
  },

  emit: async (event: string, params?: CoconutPayload) : Promise<void> => {
    await coconut.ready()
    const payloadJson = _stringifyPayload(params ?? {})

    // ack envelope is optional; if you return a JSON envelope string from C++, we parse it.
    const ack = await __coconut_emit(event, payloadJson)
    if (!ack || ack.length === 0) return

    // if ack is an envelope, treat errors properly
    try {
      const parsed = JSON.parse(ack) as CoconutCallWireEnvelope
      if (parsed && typeof parsed === 'object' && 'ok' in parsed) {
        if (parsed.ok === false) {
          throw parsed.error
        }
      }
    } catch {
      // If ack isn't JSON, just ignore.
    }
  },

  call: async <TResponse = unknown>(
    name: string,
    params?: CoconutPayload,
  ): Promise<TResponse> => {
    await coconut.ready()
    const payloadJson = _stringifyPayload(params ?? {})

    const resJson = await __coconut_call(name, payloadJson)
    const env = JSON.parse(resJson) as CoconutCallWireEnvelope

    if (env && typeof env === 'object' && 'ok' in env && env.ok === true) {
      return env.data as TResponse
    }

    // Failure
    if (env && typeof env === 'object' && 'ok' in env && env.ok === false) {
      throw env.error
    }

    throw {
      code: 'E_BRIDGE_PROTOCOL',
      message: 'Invalid response envelope from __coconut_call',
      details: env,
    } satisfies CoconutError
  },

  /**
   * Return the list of registered view names.
   */
  views: async (): Promise<string[]> => {
    await coconut.ready()
    try {
      const names = await coconut.call<string[]>("getViews", {})
      return Array.isArray(names) ? names : []
    } catch {
      return []
    }
  },

  /**
   * Ping the Lua bridge for connectivity.
   */
  ping: async (): Promise<string> => {
    return coconut.call<string>("ping", {})
  },

  /**
   * Window control helpers.
   * Usage: coconut.window.minimize(), coconut.window.toggleFullscreen(), coconut.window.close()
   */
  window: {
    minimize: async (): Promise<void> => {
      await coconut.call("__coconutWindowCtl", { cmd: "minimize" })
    },
    toggleFullscreen: async (): Promise<void> => {
      await coconut.call("__coconutWindowCtl", { cmd: "toggleFullscreen" })
    },
    close: async (): Promise<void> => {
      await coconut.call("__coconutWindowCtl", { cmd: "close" })
    },
  },

  /**
   * Filesystem helpers.
   * Usage: await coconut.fs.readText("/path/to/file")
   */
  fs: {
    readText: async (path: string): Promise<{ ok: boolean; data?: string; error?: string }> => {
      return coconut.call("fs_read_text", { path })
    },
    exists: async (path: string): Promise<{ ok: boolean; exists?: boolean; error?: string }> => {
      return coconut.call("fs_exists", { path })
    },
    writeText: async (path: string, content: string): Promise<{ ok: boolean; error?: string }> => {
      return coconut.call("fs_write_text", { path, content })
    },
    resolve: async (root: string, relpath: string): Promise<{ ok: boolean; data?: string; error?: string }> => {
      return coconut.call("fs_resolve", { root, relpath })
    },
    listDir: async (path: string): Promise<{ ok: boolean; data?: Array<{ name: string; path: string; is_dir: boolean }>; error?: string }> => {
      return coconut.call("fs_list_dir", { path })
    },
  },

  /**
   * Native dialog helpers.
   * Usage: await coconut.dialog.open("Select a file", false, true)
   */
  dialog: {
    message: async (message?: string, title?: string, kind?: string): Promise<{ confirmed: boolean }> => {
      return coconut.call("dialog_message", { message, title, kind })
    },
    open: async (title?: string, multi?: boolean, chooseDir?: boolean): Promise<{ confirmed: boolean; path: string; paths: string[] }> => {
      return coconut.call("dialog_open", { title, multi, chooseDir })
    },
    save: async (title?: string, defaultName?: string): Promise<{ confirmed: boolean; path: string }> => {
      return coconut.call("dialog_save", { title, defaultName })
    },
  },

  /**
   * Open URL in system browser.
   */
  openUrl: async (url: string): Promise<boolean> => {
    return coconut.call<boolean>("openUrl", { url })
  },

  /**
   * Show a system notification.
   */
  notify: async (title: string, body: string): Promise<boolean> => {
    return coconut.call<boolean>("notify", { title, body })
  },

  /**
   * Clipboard read/write.
   * Usage: const text = await coconut.clipboard.read()
   *       await coconut.clipboard.write("hello")
   */
  clipboard: {
    read: async (): Promise<string> => {
      return coconut.call<string>("clipboard_read", {})
    },
    write: async (text: string): Promise<boolean> => {
      return coconut.call<boolean>("clipboard_write", { text })
    },
  },
}

// ── Keybind system (hybrid chain: JS → Lua first, platform bottom-up) ─────

interface KeybindEntry {
  handler: (event: KeyboardEvent) => void
  id: string
  scope: string
  description: string
}

/** Normalize a combo string: lowercase, sort modifiers, resolve 'mod'. */
function _normalizeCombo(raw: string): string {
  // 'mod' is the canonical cross-platform modifier name.
  // Both JS and C++ use 'mod' for Cmd (macOS) / Ctrl (Windows/Linux).
  const combo = raw.toLowerCase()

  // Split into parts
  const parts = combo.split('+').map(p => p.trim())
  const key = parts.pop() ?? ''

  // Modifier priority order: mod > ctrl > alt > shift
  const modPriority: Record<string, number> = {
    mod: 0, ctrl: 1, alt: 2, shift: 3,
  }
  const modifiers = parts.filter(p => p in modPriority)
  modifiers.sort((a, b) => (modPriority[a] ?? 99) - (modPriority[b] ?? 99))

  return [...modifiers, key].join('+')
}

const _keybinds = new Map<string, KeybindEntry[]>()

/** Map special KeyboardEvent.key values to normalized form. */
function _mapKey(key: string): string {
  const map: Record<string, string> = {
    'escape': 'escape', 'tab': 'tab', 'enter': 'enter',
    ' ': 'space', 'arrowup': 'up', 'arrowdown': 'down',
    'arrowleft': 'left', 'arrowright': 'right',
    'backspace': 'backspace', 'delete': 'delete',
    'home': 'home', 'end': 'end', 'pageup': 'pageup', 'pagedown': 'pagedown',
    'insert': 'insert',
  }
  return map[key] ?? key
}

function _comboFromEvent(e: KeyboardEvent): string {
  const isMac = typeof navigator !== 'undefined' && navigator.platform.includes('Mac')
  const parts: string[] = []
  // 'mod' is the canonical cross-platform modifier (Cmd on macOS, Ctrl on Windows/Linux)
  if (e.metaKey && isMac) parts.push('mod')
  else if (e.ctrlKey && !isMac) parts.push('mod')
  else if (e.metaKey) parts.push('meta')
  else if (e.ctrlKey) parts.push('ctrl')
  if (e.altKey) parts.push('alt')
  if (e.shiftKey) parts.push('shift')

  const key = _mapKey(e.key.toLowerCase())
  parts.push(key)
  return parts.join('+')
}

function _onKeyDown(e: KeyboardEvent): void {
  const combo = _comboFromEvent(e)
  const entries = _keybinds.get(combo)
  if (!entries || entries.length === 0) {
    // Unhandled at JS level — let bubble to Lua via event
    coconut.emit('keydown.unhandled', { combo }).catch(() => {})
    return
  }

  e.preventDefault()
  e.stopPropagation()

  for (const entry of entries) {
    try {
      entry.handler(e)
    } catch (err) {
      console.error('[coconut.keybind] handler error:', err)
    }
  }

  // Notify Lua that this combo was handled at JS level
  coconut.emit('keydown', { combo, handled: true }).catch(() => {})
}

// Install DOM keydown listener (top of hybrid chain)
if (typeof document !== 'undefined') {
  document.addEventListener('keydown', _onKeyDown)
}

// Listen for bridge-delivered keydown events (from NSEvent monitor)
// When platform consumes a modifier combo, it dispatches to JS here
coconut.on('keydown', (payload) => {
  const combo = payload?.combo as string | undefined
  if (!combo) return
  const entries = _keybinds.get(combo)
  if (!entries || entries.length === 0) return
  for (const entry of entries) {
    try {
      entry.handler({} as KeyboardEvent)
    } catch (err) {
      console.error('[coconut.keybind] bridge handler error:', err)
    }
  }
})

// ── Keybind API ───────────────────────────────────────────────────────────

const keybind = Object.assign(
  function keybind(
    comboOrMap: string | Record<string, string>,
    handler: (event: KeyboardEvent) => void,
    opts?: { id?: string; scope?: string; description?: string },
  ): () => void {
    // Resolve per-platform map
    let combo: string
    if (typeof comboOrMap === 'object' && !Array.isArray(comboOrMap)) {
      const plat = typeof navigator !== 'undefined'
        ? navigator.platform.includes('Mac') ? 'mac'
        : navigator.platform.includes('Win') ? 'win'
        : 'linux'
        : 'linux'
      combo = comboOrMap[plat] ?? comboOrMap['default'] ?? ''
      if (!combo) return () => {}
    } else {
      combo = comboOrMap as string
    }

    combo = _normalizeCombo(combo)
    if (!combo) return () => {}

    const id = opts?.id ?? combo
    const scope = opts?.scope ?? 'global'
    const description = opts?.description ?? ''

    if (!_keybinds.has(combo)) _keybinds.set(combo, [])
    _keybinds.get(combo)!.push({ handler, id, scope, description })

    // Register with platform layer so NSEvent monitor consumes the event
    // (prevents macOS screen flash / Windows beep for unhandled combos)
    try {
      coconut.call('__registerPlatformKeybind', { combo, description, id }).catch(() => {})
    } catch {}

    // Return unregister function
    return () => {
      const arr = _keybinds.get(combo)
      if (!arr) return
      const idx = arr.findIndex(e => e.id === id)
      if (idx >= 0) arr.splice(idx, 1)
      if (arr.length === 0) _keybinds.delete(combo)
    }
  },
  {
    /** Override a keybind's effective combo at runtime. */
    setOverride(id: string, combo: string): void {
      // Store override in a map, re-register under new combo
      const normalized = _normalizeCombo(combo)
      _overrides.set(id, normalized)

      // Find existing entries for this id and re-register
      for (const [oldCombo, entries] of _keybinds) {
        for (const entry of entries) {
          if (entry.id === id) {
            // Remove from old combo
            const idx = entries.indexOf(entry)
            if (idx >= 0) entries.splice(idx, 1)
            if (entries.length === 0) _keybinds.delete(oldCombo)

            // Add under new combo
            if (!_keybinds.has(normalized)) _keybinds.set(normalized, [])
            _keybinds.get(normalized)!.push(entry)
            return
          }
        }
      }
    },

    /** Clear a runtime override, restoring the original combo. */
    clearOverride(id: string): void {
      _overrides.delete(id)
    },

    /** Load a table of overrides. */
    loadOverrides(table: Record<string, string>): void {
      for (const [id, combo] of Object.entries(table)) {
        _overrides.set(id, _normalizeCombo(combo))
      }
    },

    /** Get the effective combo for a keybind id. */
    getCombo(id: string): string | undefined {
      // linear scan through keybinds
      for (const [combo, entries] of _keybinds) {
        for (const entry of entries) {
          if (entry.id === id) return _overrides.get(id) ?? combo
        }
      }
      return undefined
    },
  } as {
    setOverride: (id: string, combo: string) => void
    clearOverride: (id: string) => void
    loadOverrides: (table: Record<string, string>) => void
    getCombo: (id: string) => string | undefined
  },
)

const _overrides = new Map<string, string>()

// Extend coconut with keybind API
;(coconut as Record<string, unknown>).keybind = keybind

/** Return all JS keybinds as a list of {id, combo, description, scope} */
;(coconut as Record<string, unknown>).getKeybinds = (): Array<{id: string; combo: string; description: string; scope: string}> => {
  const result: Array<{id: string; combo: string; description: string; scope: string}> = []
  for (const [combo, entries] of _keybinds) {
    for (const entry of entries) {
      result.push({ id: entry.id, combo, description: entry.description, scope: entry.scope })
    }
  }
  return result
}

// Expose globally so injected <script> (non-module) can access `window.coconut`.
;(globalThis as any).coconut = coconut


