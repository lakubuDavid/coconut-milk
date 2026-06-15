/* Coconut Milk — Shared Scripts */

// ── Window controls (frameless titlebar buttons) ──
function doMinimize() {
  if (window.coconut && window.coconut.window)
    coconut.window.minimize();
}
function doToggleFullscreen() {
  if (window.coconut && window.coconut.window)
    coconut.window.toggleFullscreen();
}
function doClose() {
  if (window.coconut && window.coconut.window)
    coconut.window.close();
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

// ── Dynamic navigation: populate <nav> from registered views ──
(async function() {
  try {
    await coconut.ready();

    const viewNames = await coconut.views();
    if (!viewNames || viewNames.length === 0) return;

    // Get current view name from body data attribute or nav active link
    const currentView =
      document.body.dataset.view ||
      document.querySelector('nav a.active')?.dataset?.view;

    // Find all <nav> elements, or create one if missing
    const navs = document.querySelectorAll('nav');
    if (navs.length === 0) return;

    for (const nav of navs) {
      // Skip navs that already have dynamic links (e.g. sidebar title)
      if (nav.dataset.dynamic === 'done') continue;
      nav.dataset.dynamic = 'done';

      // Only populate empty navs or navs with a heading but no view links
      const existingLinks = nav.querySelectorAll('a[data-view]');
      if (existingLinks.length > 0) {
        // Nav already has hardcoded links — just ensure click handler
        existingLinks.forEach(a => {
          a.addEventListener('click', e => {
            e.preventDefault();
            const view = a.dataset.view;
            if (window.coconut && window.coconut.emit)
              window.coconut.emit('navigate', { view });
          });
        });
        continue;
      }

      // Generate links for each registered view
      for (const name of viewNames) {
        const a = document.createElement('a');
        a.href = '#';
        a.dataset.view = name;
        a.textContent = name.charAt(0).toUpperCase() + name.slice(1);
        if (name === currentView) a.className = 'active';
        a.addEventListener('click', (e) => {
          e.preventDefault();
          if (window.coconut && window.coconut.emit)
            window.coconut.emit('navigate', { view: name });
          // Update active class
          nav.querySelectorAll('a[data-view]').forEach(link => link.classList.remove('active'));
          a.classList.add('active');
        });
        nav.appendChild(a);
      }
    }
  } catch (e) {
    // bridge or views unavailable — hardcoded links fall back
  }
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
