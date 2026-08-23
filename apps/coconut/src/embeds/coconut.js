// src/embeds/coconut.ts
class CoconutEvent {
  name;
  target;
  defaultPrevented = false;
  propagationStopped = false;
  constructor(name, payload, target) {
    this.name = name;
    this.target = target;
    Object.assign(this, payload);
  }
  get type() {
    return this.name;
  }
  preventDefault() {
    this.defaultPrevented = true;
  }
  stopPropagation() {
    this.propagationStopped = true;
  }
  stopImmediatePropagation() {
    this.defaultPrevented = true;
    this.propagationStopped = true;
  }
}
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
globalThis.__coconut_dispatch_event = (name, payloadJson, target) => {
  const entries = _listeners.get(name);
  if (!entries || entries.length === 0)
    return;
  let payload = {};
  if (payloadJson && payloadJson.length > 0) {
    try {
      payload = JSON.parse(payloadJson);
    } catch {
      payload = {};
    }
  }
  const event = new CoconutEvent(name, payload, target);
  const snapshot = entries.slice();
  for (const entry of snapshot) {
    const idx = entries.indexOf(entry);
    if (idx === -1)
      continue;
    try {
      entry.fn(event);
    } catch {}
    if (entry.once) {
      const rmIdx = entries.indexOf(entry);
      if (rmIdx >= 0)
        entries.splice(rmIdx, 1);
    }
    if (event.propagationStopped)
      break;
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
  on: (event, callbackFn, opts) => {
    let entries = _listeners.get(event);
    if (!entries) {
      entries = [];
      _listeners.set(event, entries);
    }
    const entry = { fn: callbackFn, once: opts?.once === true };
    entries.push(entry);
    return () => {
      const idx = entries.indexOf(entry);
      if (idx >= 0)
        entries.splice(idx, 1);
    };
  },
  emit: async (event, legacyPayload) => {
    await coconut.ready();
    const ev = typeof event === "string" ? { name: event, ...legacyPayload ?? {} } : event;
    const name = ev.name;
    if (!name)
      throw new Error("event must have a 'name' field");
    const { name: _, ...payload } = ev;
    const payloadJson = _stringifyPayload(payload);
    const ack = await __coconut_emit(name, payloadJson);
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
    const payloadJson = _stringifyPayload(params ?? {});
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
  },
  views: async () => {
    await coconut.ready();
    try {
      const names = await coconut.call("getViews", {});
      return Array.isArray(names) ? names : [];
    } catch {
      return [];
    }
  },
  ping: async () => {
    return coconut.call("ping", {});
  },
  window: {
    minimize: async () => {
      await coconut.call("__coconutWindowCtl", { cmd: "minimize" });
    },
    toggleFullscreen: async () => {
      await coconut.call("__coconutWindowCtl", { cmd: "toggleFullscreen" });
    },
    close: async () => {
      await coconut.call("__coconutWindowCtl", { cmd: "close" });
    }
  },
  fs: {
    readText: async (path) => {
      return coconut.call("fs_read_text", { path });
    },
    exists: async (path) => {
      return coconut.call("fs_exists", { path });
    },
    writeText: async (path, content) => {
      return coconut.call("fs_write_text", { path, content });
    },
    resolve: async (root, relpath) => {
      return coconut.call("fs_resolve", { root, relpath });
    },
    listDir: async (path) => {
      return coconut.call("fs_list_dir", { path });
    }
  },
  dialog: {
    message: async (message, title, kind) => {
      return coconut.call("dialog_message", { message, title, kind });
    },
    open: async (title, multi, chooseDir) => {
      return coconut.call("dialog_open", { title, multi, chooseDir });
    },
    save: async (title, defaultName) => {
      return coconut.call("dialog_save", { title, defaultName });
    }
  },
  openUrl: async (url) => {
    return coconut.call("openUrl", { url });
  },
  notify: async (title, body) => {
    return coconut.call("notify", { title, body });
  },
  clipboard: {
    read: async () => {
      return coconut.call("clipboard_read", {});
    },
    write: async (text) => {
      return coconut.call("clipboard_write", { text });
    }
  },
  store: {
    set: async (key, value) => {
      return coconut.call("store_set", { key, value });
    },
    get: async (key) => {
      const result = await coconut.call("store_get", { key });
      return result.value ?? "";
    },
    has: async (key) => {
      const result = await coconut.call("store_has", { key });
      return result.has ?? false;
    },
    delete: async (key) => {
      return coconut.call("store_delete", { key });
    },
    clear: async () => {
      return coconut.call("store_clear", {});
    },
    keys: async () => {
      const result = await coconut.call("store_keys", {});
      return result.keys ?? [];
    }
  },
  quit: () => {
    coconut.window.close();
  }
};
function _normalizeCombo(raw) {
  const combo = raw.toLowerCase();
  const parts = combo.split("+").map((p) => p.trim());
  const key = parts.pop() ?? "";
  const modPriority = {
    mod: 0,
    ctrl: 1,
    alt: 2,
    shift: 3
  };
  const modifiers = parts.filter((p) => (p in modPriority));
  modifiers.sort((a, b) => (modPriority[a] ?? 99) - (modPriority[b] ?? 99));
  return [...modifiers, key].join("+");
}
var _keybinds = new Map;
function _mapKey(key) {
  const map = {
    escape: "escape",
    tab: "tab",
    enter: "enter",
    " ": "space",
    arrowup: "up",
    arrowdown: "down",
    arrowleft: "left",
    arrowright: "right",
    backspace: "backspace",
    delete: "delete",
    home: "home",
    end: "end",
    pageup: "pageup",
    pagedown: "pagedown",
    insert: "insert"
  };
  return map[key] ?? key;
}
function _comboFromEvent(e) {
  const isMac = typeof navigator !== "undefined" && navigator.platform.includes("Mac");
  const parts = [];
  if (e.metaKey && isMac)
    parts.push("mod");
  else if (e.ctrlKey && !isMac)
    parts.push("mod");
  else if (e.metaKey)
    parts.push("meta");
  else if (e.ctrlKey)
    parts.push("ctrl");
  if (e.altKey)
    parts.push("alt");
  if (e.shiftKey)
    parts.push("shift");
  const key = _mapKey(e.key.toLowerCase());
  parts.push(key);
  return parts.join("+");
}
function _onKeyDown(e) {
  const combo = _comboFromEvent(e);
  const entries = _keybinds.get(combo);
  if (!entries || entries.length === 0) {
    coconut.emit({ name: "keydown.unhandled", combo }).catch(() => {});
    return;
  }
  e.preventDefault();
  e.stopPropagation();
  for (const entry of entries) {
    try {
      entry.handler(e);
    } catch (err) {
      console.error("[coconut.keybind] handler error:", err);
    }
  }
  coconut.emit({ name: "keydown", combo, handled: true }).catch(() => {});
}
if (typeof document !== "undefined") {
  document.addEventListener("keydown", _onKeyDown);
}
coconut.on("keydown", (event) => {
  const combo = event.combo;
  if (!combo)
    return;
  const entries = _keybinds.get(combo);
  if (!entries || entries.length === 0)
    return;
  for (const entry of entries) {
    try {
      entry.handler({});
    } catch (err) {
      console.error("[coconut.keybind] bridge handler error:", err);
    }
  }
});
var keybind = Object.assign(function keybind2(comboOrMap, handler, opts) {
  let combo;
  if (typeof comboOrMap === "object" && !Array.isArray(comboOrMap)) {
    const plat = typeof navigator !== "undefined" ? navigator.platform.includes("Mac") ? "mac" : navigator.platform.includes("Win") ? "win" : "linux" : "linux";
    combo = comboOrMap[plat] ?? comboOrMap["default"] ?? "";
    if (!combo)
      return () => {};
  } else {
    combo = comboOrMap;
  }
  combo = _normalizeCombo(combo);
  if (!combo)
    return () => {};
  const id = opts?.id ?? combo;
  const scope = opts?.scope ?? "global";
  const description = opts?.description ?? "";
  if (!_keybinds.has(combo))
    _keybinds.set(combo, []);
  _keybinds.get(combo).push({ handler, id, scope, description });
  try {
    coconut.call("__registerPlatformKeybind", { combo, description, id }).catch(() => {});
  } catch {}
  return () => {
    const arr = _keybinds.get(combo);
    if (!arr)
      return;
    const idx = arr.findIndex((e) => e.id === id);
    if (idx >= 0)
      arr.splice(idx, 1);
    if (arr.length === 0)
      _keybinds.delete(combo);
  };
}, {
  setOverride(id, combo) {
    const normalized = _normalizeCombo(combo);
    _overrides.set(id, normalized);
    for (const [oldCombo, entries] of _keybinds) {
      for (const entry of entries) {
        if (entry.id === id) {
          const idx = entries.indexOf(entry);
          if (idx >= 0)
            entries.splice(idx, 1);
          if (entries.length === 0)
            _keybinds.delete(oldCombo);
          if (!_keybinds.has(normalized))
            _keybinds.set(normalized, []);
          _keybinds.get(normalized).push(entry);
          return;
        }
      }
    }
  },
  clearOverride(id) {
    _overrides.delete(id);
  },
  loadOverrides(table) {
    for (const [id, combo] of Object.entries(table)) {
      _overrides.set(id, _normalizeCombo(combo));
    }
  },
  getCombo(id) {
    for (const [combo, entries] of _keybinds) {
      for (const entry of entries) {
        if (entry.id === id)
          return _overrides.get(id) ?? combo;
      }
    }
    return;
  }
});
var _overrides = new Map;
coconut.keybind = keybind;
coconut.getKeybinds = () => {
  const result = [];
  for (const [combo, entries] of _keybinds) {
    for (const entry of entries) {
      result.push({ id: entry.id, combo, description: entry.description, scope: entry.scope });
    }
  }
  return result;
};
globalThis.coconut = coconut;
var _pendingErrors = [];
function _flushPendingErrors() {
  const pending = _pendingErrors;
  _pendingErrors = [];
  for (const err of pending) {
    coconut.call("_js_log", err).catch(() => {});
  }
}
window.onerror = (_event, _source, _lineno, _colno, _error) => {
  const msg = typeof _event === "string" ? _event : _event?.toString() ?? "Unknown script error";
  const stack = _error?.stack || `${_source ?? "?"}:${_lineno ?? 0}:${_colno ?? 0}`;
  const entry = { level: "error", message: `[JS] ${msg}`, stack };
  if (!_ready) {
    _pendingErrors.push(entry);
    return false;
  }
  coconut.call("_js_log", entry).catch(() => {});
  return false;
};
window.addEventListener("unhandledrejection", (e) => {
  const reason = e.reason;
  const msg = reason?.message ?? String(reason);
  const stack = reason?.stack ?? "";
  const entry = { level: "error", message: `[JS] Unhandled Promise rejection: ${msg}`, stack };
  if (!_ready) {
    _pendingErrors.push(entry);
    return;
  }
  coconut.call("_js_log", entry).catch(() => {});
});
coconut.ready().then(() => _flushPendingErrors());
keybind({ mac: "mod+q", win: "alt+f4", linux: "alt+f4" }, () => {
  coconut.window.close();
}, { id: "coconut.window.close", description: "Close window" });
