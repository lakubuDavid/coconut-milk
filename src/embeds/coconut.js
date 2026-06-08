// src/embeds/coconut.ts
var _listeners = new Map;
var _ready = false;
var _readyResolve;
var _readyPromise = new Promise((resolve) => {
  _readyResolve = resolve;
});
function _markReady() {
  if (_ready)
    return;
  _ready = true;
  _readyResolve?.();
}
globalThis.__coconut_bridge_ready = () => {
  _markReady();
};
globalThis.__coconut_rpc_receive = (_msgJson) => {};
globalThis.__coconut_dispatch_event = (name, payloadJson) => {
  const set = _listeners.get(name);
  if (!set || set.size === 0)
    return;
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
    } catch {}
  }
};
function _stringifyPayload(payload) {
  return JSON.stringify(payload ?? {});
}
var coconut = {
  ready: async () => {
    if (_ready)
      return;
    await _readyPromise;
  },
  on: (event, callbackFn) => {
    let set = _listeners.get(event);
    if (!set) {
      set = new Set;
      _listeners.set(event, set);
    }
    set.add(callbackFn);
    return () => {
      set?.delete(callbackFn);
    };
  },
  emit: async (event, params) => {
    await coconut.ready();
    const payloadJson = _stringifyPayload(params);
    const ack = await __coconut_emit(event, payloadJson);
    if (!ack || ack.length === 0)
      return;
    try {
      const parsed = JSON.parse(ack);
      if (parsed && typeof parsed === "object" && "ok" in parsed) {
        if (parsed.ok === false) {
          throw parsed.error;
        }
      }
    } catch {}
  },
  call: async (name, params) => {
    await coconut.ready();
    const payloadJson = _stringifyPayload(params);
    const resJson = await __coconut_call(name, payloadJson);
    const env = JSON.parse(resJson);
    if (env && typeof env === "object" && "ok" in env && env.ok === true) {
      return env.data;
    }
    if (env && typeof env === "object" && "ok" in env && env.ok === false) {
      throw env.error;
    }
    throw {
      code: "E_BRIDGE_PROTOCOL",
      message: "Invalid response envelope from __coconut_call",
      details: env
    };
  }
};
globalThis.coconut = coconut;
