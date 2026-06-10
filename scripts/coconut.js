/** Coconut runtime bridge (generated/handwritten twin of coconut.ts)
 *  Keep it compatible with src/embeds/coconut.ts.
 */

const listeners = new Map();
let ready = false;
let readyResolve;
const readyPromise = new Promise((resolve) => {
  readyResolve = resolve;
});

/** Called by injected JS from C++ to mark the bridge as ready. */
globalThis.__coconut_bridge_ready = () => {
  if (ready) return;
  ready = true;
  readyResolve();
};

/** Called by injected JS from C++ to deliver Lua -> JS events. */
globalThis.__coconut_dispatch_event = (name, payloadJson) => {
  const set = listeners.get(name);
  if (!set || set.size === 0) return;

  let payload = {};
  if (payloadJson && payloadJson.length > 0) {
    try {
      payload = JSON.parse(payloadJson);
    } catch {
      payload = {};
    }
  }

  for (const cb of Array.from(set)) {
    try {
      cb(payload);
    } catch {
      // ignore
    }
  }
};

async function _ready() {
  if (ready) return;
  await readyPromise;
}

/**
 * Register a frontend listener for a Lua-emitted event.
 *
 * @returns unsubscribe function
 */
function on(event, callbackFn) {
  let set = listeners.get(event);
  if (!set) {
    set = new Set();
    listeners.set(event, set);
  }
  set.add(callbackFn);
  return () => {
    set.delete(callbackFn);
  };
}

/**
 * Send a frontend event into Lua.
 *
 * @returns Promise<void>
 */
async function emit(event, params) {
  await _ready();
  const payloadJson = JSON.stringify(params || {});
  const ack = await globalThis.__coconut_emit(event, payloadJson);
  if (!ack || ack.length === 0) return;
  try {
    const parsed = JSON.parse(ack);
    if (parsed && parsed.ok === false) {
      throw parsed.error;
    }
  } catch {
    // ignore invalid ack
  }
}

/**
 * Call a bound Lua command.
 *
 * @returns Promise with the command's JSON `data` value
 */
async function call(name, params) {
  await _ready();
  const payloadJson = JSON.stringify(params || {});
  const resJson = await globalThis.__coconut_call(name, payloadJson);
  const env = JSON.parse(resJson);
  if (env && env.ok === true) return env.data;
  if (env && env.ok === false) throw env.error;
  throw { code: 'E_BRIDGE_PROTOCOL', message: 'Invalid response envelope' };
}

export const coconut = {
  ready: _ready,
  on,
  emit,
  call,

  /** Get all registered view names. */
  views: async () => {
    await _ready();
    try {
      const names = await call("getViews", {});
      return Array.isArray(names) ? names : [];
    } catch {
      return [];
    }
  },

  /** Ping the Lua bridge for connectivity. */
  ping: async () => {
    return call("ping", {});
  },

  /** Window control helpers. */
  window: {
    minimize: async () => {
      await call("__coconut_window_ctl", { cmd: "minimize" });
    },
    toggleFullscreen: async () => {
      await call("__coconut_window_ctl", { cmd: "toggleFullscreen" });
    },
    close: async () => {
      await call("__coconut_window_ctl", { cmd: "close" });
    },
  },

  /** Filesystem helpers. */
  fs: {
    readText: async (path) => {
      return call("fs_read_text", { path });
    },
  },
};
