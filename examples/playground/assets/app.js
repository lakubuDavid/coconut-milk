// Coconut Milk Playground — interactive feature test harness.
// Calls all bridge commands and displays results inline.

/* global coconut */

// ── Helpers ──────────────────────────────────────────────────────────────

const $ = (sel) => document.querySelector(sel)
const $$ = (sel) => document.querySelectorAll(sel)

const LOG = $('#log-area')
const STATUS = $('#status-badge')

function status(state) {
  STATUS.textContent = state
  STATUS.className = 'badge ' + state
}

function log(msg) {
  const t = new Date().toLocaleTimeString()
  LOG.textContent += `[${t}] ${msg}\n`
  LOG.scrollTop = LOG.scrollHeight
}

async function call(cmd, payload = {}) {
  log(`→ ${cmd}(${JSON.stringify(payload)})`)
  try {
    const res = await coconut.call(cmd, payload)
    log(`← ${JSON.stringify(res)}`)
    return res
  } catch (e) {
    const msg = e?.message || String(e)
    log(`✗ ${msg}`)
    throw e
  }
}

function show(el, text, ok = true) {
  const out = $(el)
  out.textContent = text
  out.className = 'result' + (ok ? '' : ' error')
}

function showPre(el, text) {
  const out = $(el)
  out.textContent = typeof text === 'string' ? text : JSON.stringify(text, null, 2)
  out.className = 'result pre'
}

// ── Tab switching ────────────────────────────────────────────────────────

$('#tabs').addEventListener('click', (e) => {
  const btn = e.target.closest('.tab')
  if (!btn) return

  $$('.tab').forEach(t => { t.classList.remove('active'); t.ariaSelected = 'false' })
  $$('.panel').forEach(p => p.classList.remove('active'))

  btn.classList.add('active')
  btn.ariaSelected = 'true'

  const panel = $('#panel-' + btn.dataset.tab)
  if (panel) panel.classList.add('active')
})

// ── Button dispatcher ────────────────────────────────────────────────────

document.addEventListener('click', async (e) => {
  const btn = e.target.closest('[data-cmd]')
  if (!btn) return
  e.preventDefault()
  btn.disabled = true
  btn.textContent = '…'
  try {
    await CMD[btn.dataset.cmd](btn)
  } catch { /* already logged */ }
  btn.disabled = false
  btn.textContent = btn.dataset.label || btn.textContent.replace('…', '')
})

// ── Command implementations ──────────────────────────────────────────────

const CMD = {}

// ── System ───────────────────────────────────────────────────────────────

CMD.env_home = async () => {
  const env = await call('playground_env')
  show('#r-env', `HOME = ${env.HOME}`, true)
}
CMD.env_user = async () => {
  const env = await call('playground_env')
  show('#r-env', `USER = ${env.USER}`, true)
}
CMD.env_cwd = async () => {
  const env = await call('playground_env')
  show('#r-env', `cwd = ${env.cwd}`, true)
}
CMD.env_homedir = async () => {
  const env = await call('playground_env')
  show('#r-env', `homedir = ${env.homedir}`, true)
}
CMD.env_sep = async () => {
  const env = await call('playground_env')
  show('#r-env', `pathSeparator = "${env.pathSeparator}"`, true)
}

CMD.open_url = async () => {
  const url = $('#url-input').value.trim()
  if (!url) return show('#r-url', 'Enter a URL', false)
  const ok = await call('openUrl', { url })
  show('#r-url', ok ? `Opened: ${url}` : 'Failed to open URL', ok)
}

CMD.notify = async () => {
  const title = $('#notif-title').value.trim() || 'Coconut'
  const body  = $('#notif-body').value.trim()  || 'test'
  const ok = await call('notify', { title, body })
  show('#r-notify', ok ? 'Notification sent' : 'Failed', ok)
}

CMD.log_info  = async () => { coconut.info?.('playground info');  show('#r-log', '→ info logged') }
CMD.log_warn  = async () => { coconut.warn?.('playground warn');  show('#r-log', '→ warn logged') }
CMD.log_error = async () => { coconut.error?.('playground error'); show('#r-log', '→ error logged') }
CMD.log_debug = async () => { coconut.log?.('playground debug');  show('#r-log', '→ debug logged') }

// ── Clipboard ────────────────────────────────────────────────────────────

CMD.cb_read = async () => {
  const text = await call('clipboard_read')
  show('#r-cb-read', text ? `"${text}"` : '(empty)', true)
}

CMD.cb_write = async () => {
  const text = $('#cb-write-input').value
  const ok = await call('clipboard_write', { text })
  show('#r-cb-write', ok ? `Written: "${text}"` : 'Write failed', ok)
}

CMD.cb_read_after = async () => {
  const text = await call('clipboard_read')
  show('#r-cb-write', `Read back: "${text}"`, true)
}

// ── Dialogs ──────────────────────────────────────────────────────────────

CMD.dlg_message = async () => {
  const msg   = $('#dlg-msg').value || 'Hello'
  const kind  = $('#dlg-kind').value
  const res   = await call('dialog_message', { message: msg, title: 'Playground', kind })
  show('#r-dlg-msg', `confirmed: ${res.confirmed}`, true)
}

CMD.dlg_open_file = async () => {
  const res = await call('dialog_open', { title: 'Pick a file', multi: false, chooseDir: false })
  show('#r-dlg-open', res.confirmed ? `File: ${res.path}` : '(cancelled)', res.confirmed)
}

CMD.dlg_open_dir = async () => {
  const res = await call('dialog_open', { title: 'Pick a folder', multi: false, chooseDir: true })
  show('#r-dlg-open', res.confirmed ? `Folder: ${res.path}` : '(cancelled)', res.confirmed)
}

CMD.dlg_open_multi = async () => {
  const res = await call('dialog_open', { title: 'Select files', multi: true, chooseDir: false })
  if (res.confirmed) {
    show('#r-dlg-open', `Files (${res.paths.length}):\n${res.paths.join('\n')}`, true)
  } else {
    show('#r-dlg-open', '(cancelled)', false)
  }
}

CMD.dlg_save = async () => {
  const name = $('#dlg-save-name').value || 'untitled.txt'
  const res = await call('dialog_save', { title: 'Save file', defaultName: name })
  show('#r-dlg-save', res.confirmed ? `Save to: ${res.path}` : '(cancelled)', res.confirmed)
}

// ── File System ──────────────────────────────────────────────────────────

CMD.fs_read = async () => {
  const path = $('#fs-read-path').value.trim()
  if (!path) return show('#r-fs-read', 'Enter a path', false)
  try {
    const res = await call('fs_read_text', { path })
    if (res.ok) showPre('#r-fs-read', res.data)
    else showPre('#r-fs-read', `Error: ${res.error || 'unknown'}`)
  } catch (e) {
    showPre('#r-fs-read', `Error: ${e.message || e}`)
  }
}

CMD.fs_write = async () => {
  const path    = $('#fs-write-path').value.trim() || '_test.txt'
  const content = $('#fs-write-content').value
  const res = await call('fs_write_text', { path, content })
  show('#r-fs-write', res.ok ? `Written to ${path}` : `Error: ${res.error}`, res.ok)
}

CMD.fs_exists = async () => {
  const path = $('#fs-read-path').value.trim() || 'coconut.config.lua'
  const res = await call('fs_exists', { path })
  show('#r-fs-ops', res.exists ? `Exists: ${path}` : `Not found: ${path}`, res.exists)
}

CMD.fs_resolve = async () => {
  const root = $('#fs-read-path').value.trim() || '.'
  const relpath = 'main.lua'
  const res = await call('fs_resolve', { root, relpath })
  show('#r-fs-ops', res.ok ? `Resolved: ${res.data}` : `Error: ${res.error}`, res.ok)
}

CMD.fs_listdir = async () => {
  const path = $('#fs-read-path').value.trim() || '.'
  try {
    const res = await call('fs_list_dir', { path })
    if (res.ok && res.data) {
      const lines = res.data.map(e => `${e.is_dir ? '📁' : '📄'} ${e.name}`).join('\n')
      showPre('#r-fs-ops', lines || '(empty directory)')
    } else {
      show('#r-fs-ops', `Error: ${res.error || 'unknown'}`, false)
    }
  } catch (e) {
    show('#r-fs-ops', `Error: ${e.message || e}`, false)
  }
}

// ── Window ───────────────────────────────────────────────────────────────

CMD.win_minimize = () => call('__coconut_window_ctl', { cmd: 'minimize' }).then(() => show('#r-win', 'window minimized'))
CMD.win_maximize = () => call('__coconut_window_ctl', { cmd: 'maximize' }).then(() => show('#r-win', 'window maximized'))
CMD.win_fullscreen_on  = () => call('__coconut_window_ctl', { cmd: 'fullscreen_on' }).then(() => show('#r-win', 'fullscreen on'))
CMD.win_fullscreen_off = () => call('__coconut_window_ctl', { cmd: 'fullscreen_off' }).then(() => show('#r-win', 'fullscreen off'))

CMD.win_resize_small = () => call('__coconut_window_ctl', { cmd: 'resize', w: 640, h: 480 }).then(() => show('#r-win', 'resized to 640×480'))
CMD.win_resize_large = () => call('__coconut_window_ctl', { cmd: 'resize', w: 1024, h: 768 }).then(() => show('#r-win', 'resized to 1024×768'))

CMD.win_setpos = () => {
  const x = +$('#win-x').value
  const y = +$('#win-y').value
  return call('__coconut_window_ctl', { cmd: 'setPosition', x, y }).then(() => show('#r-win-pos', `moved to (${x}, ${y})`))
}

CMD.win_resize = () => {
  const w = +$('#win-w').value
  const h = +$('#win-h').value
  return call('__coconut_window_ctl', { cmd: 'resize', w, h }).then(() => show('#r-win-pos', `resized to ${w}×${h}`))
}

CMD.win_reload = async () => {
  show('#r-win-reload', 'reloading…')
  try {
    await coconut.call('__coconut_window_ctl', { cmd: 'reload' })
  } catch {
    // maybe no reload command — just log
    log('reload: view will need manual refresh')
  }
  show('#r-win-reload', 'reload triggered')
}

CMD.win_debug = async () => {
  try {
    const info = await call('__coconut_window_ctl', { cmd: 'debug' })
    show('#r-win', JSON.stringify(info, null, 2))
  } catch (e) {
    show('#r-win', `debug error: ${e.message || e}`, false)
  }
}

// ── JSON ─────────────────────────────────────────────────────────────────

CMD.json_parse = () => {
  const raw = $('#json-input').value.trim()
  try {
    const obj = JSON.parse(raw)
    showPre('#r-json', JSON.stringify(obj, null, 2))
  } catch (e) {
    show('#r-json', `Parse error: ${e.message}`, false)
  }
}

CMD.json_roundtrip = async () => {
  const raw = $('#json-input').value.trim()
  try {
    const parsed = JSON.parse(raw)
    const res = await call('playground_json', { payload: raw })
    showPre('#r-json', `→ sent:  ${raw}\n\n← received:\n${JSON.stringify(JSON.parse(res), null, 2)}`)
  } catch (e) {
    show('#r-json', `Error: ${e.message}`, false)
  }
}

// ── Events ───────────────────────────────────────────────────────────────

CMD.echo_cmd = async () => {
  const val = $('#echo-input').value
  const res = await call('playground_echo', { value: val })
  show('#r-echo', `echoed: "${res.echoed}" at ${res.received_at}`, true)
}

let listening = false

CMD.evt_listen = async () => {
  if (listening) {
    show('#r-evts', 'already listening', true)
    return
  }
  listening = true
  coconut.on('playground_event', (payload) => {
    log(`📨 event: playground_event → ${JSON.stringify(payload)}`)
    show('#r-evts', `Received: message="${payload.message}" count=${payload.count}`, true)
  })
  show('#r-evts', 'listening for playground_event…', true)
}

CMD.evt_send = async () => {
  const msg = $('#evt-input').value || 'hello'
  await call('playground_send_event', { message: msg, count: Math.floor(Math.random() * 100) })
  show('#r-evts', `sent event with message="${msg}"`, true)
}

// ── Log clear ────────────────────────────────────────────────────────────

CMD.log_clear = () => { LOG.textContent = 'Cleared.\n' }

// ── Init ─────────────────────────────────────────────────────────────────

async function init() {
  log('connecting…')
  try {
    await coconut.ready()
    log('coconut ready')
    status('connected')

    // Quick ping test
    const pong = await call('ping')
    log(`ping → ${pong}`)
    show('#r-env', `Coconut bridge active (ping: ${pong})`, true)
  } catch (e) {
    log(`init error: ${e}`)
    status('error')
  }
}

init()
