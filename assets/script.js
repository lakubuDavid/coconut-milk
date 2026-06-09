/* Coconut Milk — Shared Scripts */

// ── Window controls (frameless titlebar buttons) ──
function doMinimize() {
  if (window.coconut && window.coconut.call)
    coconut.call('__coconut_window_ctl', { cmd: 'minimize' });
}
function doToggleFullscreen() {
  if (window.coconut && window.coconut.call)
    coconut.call('__coconut_window_ctl', { cmd: 'toggleFullscreen' });
}
function doClose() {
  if (window.coconut && window.coconut.call)
    coconut.call('__coconut_window_ctl', { cmd: 'close' });
}

// ── Drag region: emit grab events on titlebar ──
(function() {
  let grabbing = false;
  let lastX = 0, lastY = 0;
  const tb = document.getElementById('titlebar');

  if (tb) {
    tb.addEventListener('mousedown', (e) => {
      grabbing = true;
      lastX = e.screenX;
      lastY = e.screenY;
      if (window.coconut && window.coconut.emit)
        window.coconut.emit('grab_start', {});
    });
  }

  document.addEventListener('mousemove', (e) => {
    if (!grabbing) return;
    const dx = e.screenX - lastX;
    const dy = e.screenY - lastY;
    lastX = e.screenX;
    lastY = e.screenY;
    if (window.coconut && window.coconut.emit)
      window.coconut.emit('move', { dx, dy });
  });

  document.addEventListener('mouseup', () => {
    if (!grabbing) return;
    grabbing = false;
    if (window.coconut && window.coconut.emit)
      window.coconut.emit('grab_end', {});
  });
})();

// ── Navigation: emit view-switch events ──
(function() {
  document.querySelectorAll('nav a').forEach(a => {
    a.addEventListener('click', e => {
      e.preventDefault();
      const view = a.dataset.view;
      if (window.coconut && window.coconut.emit)
        window.coconut.emit('navigate', { view });
    });
  });
})();

// ── Bridge ready handshake ──
(async function() {
  try {
    await coconut.ready();
    const el = document.getElementById('bridge-status');
    if (el) {
      el.textContent = 'connected';
      el.className = 'connected';
    }
  } catch (e) {
    // bridge not available
  }
})();
