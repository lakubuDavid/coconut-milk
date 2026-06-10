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
 * Called by the WebUI transport to deliver kReturn/kError RPC responses.
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
      await coconut.call("__coconut_window_ctl", { cmd: "minimize" })
    },
    toggleFullscreen: async (): Promise<void> => {
      await coconut.call("__coconut_window_ctl", { cmd: "toggleFullscreen" })
    },
    close: async (): Promise<void> => {
      await coconut.call("__coconut_window_ctl", { cmd: "close" })
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
  },
}

// Expose globally so injected <script> (non-module) can access `window.coconut`.
;(globalThis as any).coconut = coconut


