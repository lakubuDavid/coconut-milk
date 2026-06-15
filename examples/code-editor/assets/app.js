// Coconut Code Editor — client-side logic (CodeMirror 6)
// Buffer system: multiple open files with tab bar, dirty tracking,
// and persistent scroll/cursor state per buffer.
(function () {
  'use strict';

  // ── Buffer class ──────────────────────────────────────────────────
  // An in-memory representation of a file's content and metadata.
  // Buffers persist even when not visible — only destroyed on explicit close.
  let _nextBufferId = 1;
  class Buffer {
    constructor(path, name, content, language, type) {
      this.id        = _nextBufferId++;
      this.path      = path;
      this.name      = name;
      this.content   = content || '';
      this.language  = language || 'text';
      this.type      = type || 'text';      // 'text' | 'image'
      this.dirty     = false;
      this.cursorPos = { line: 0, col: 0 };
      this.scrollPos = 0;
      this.created   = Date.now();
    }
    // Update content and mark dirty
    setContent(text) {
      if (text !== this.content) {
        this.content = text;
        this.dirty = true;
      }
    }
    markClean() { this.dirty = false; }
  }

  // ── BufferManager class ──────────────────────────────────────────
  // Owns all open buffers, tracks the active one, provides navigation.
  class BufferManager {
    constructor() {
      this._buffers  = [];           // Buffer[]
      this._activeId = null;         // id of the active buffer
    }

    get active() { return this._buffers.find(b => b.id === this._activeId) || null; }
    get count()  { return this._buffers.length; }
    get all()    { return this._buffers.slice(); } // copy

    getById(id)  { return this._buffers.find(b => b.id === id) || null; }

    /** Find an existing buffer by path, or create+add a new one. */
    open(path, name, content, language, type) {
      const existing = this._buffers.find(b => b.path === path);
      if (existing) {
        // Update content if provided (e.g. after reload)
        if (content !== undefined) existing.content = content;
        if (language) existing.language = language;
        if (type) existing.type = type;
        return existing;
      }
      const buf = new Buffer(path, name, content, language, type);
      this._buffers.push(buf);
      return buf;
    }

    /** Add a new unnamed/untitled buffer. */
    createUntitled() {
      const name = 'untitled-' + this.count;
      const buf = new Buffer('', name, '', 'text', 'text');
      buf.dirty = true; // untitled buffers always need saving
      this._buffers.push(buf);
      return buf;
    }

    /** Close a buffer by id.  Returns the buffer that becomes active (or null). */
    close(id) {
      const idx = this._buffers.findIndex(b => b.id === id);
      if (idx === -1) return null;
      this._buffers.splice(idx, 1);
      // If we closed the active buffer, switch to another
      if (this._activeId === id) {
        if (this._buffers.length > 0) {
          const nextIdx = Math.min(idx, this._buffers.length - 1);
          this._activeId = this._buffers[nextIdx].id;
        } else {
          this._activeId = null;
        }
      }
      return this.active;
    }

    /** Set the active buffer by id. */
    setActive(id) {
      if (this.getById(id)) {
        this._activeId = id;
        return true;
      }
      return false;
    }

    /** Switch to the next buffer in the list (cyclical). */
    next() {
      if (this._buffers.length === 0) return null;
      const idx = this._buffers.findIndex(b => b.id === this._activeId);
      const nextIdx = (idx + 1) % this._buffers.length;
      this._activeId = this._buffers[nextIdx].id;
      return this.active;
    }

    /** Switch to the previous buffer in the list (cyclical). */
    prev() {
      if (this._buffers.length === 0) return null;
      const idx = this._buffers.findIndex(b => b.id === this._activeId);
      const prevIdx = (idx - 1 + this._buffers.length) % this._buffers.length;
      this._activeId = this._buffers[prevIdx].id;
      return this.active;
    }

    /** Return all dirty (unsaved) buffers. */
    getUnsaved() {
      return this._buffers.filter(b => b.dirty);
    }
  }

  // ── State ─────────────────────────────────────────────────────────
  const buffers = new BufferManager();
  let editorView = null;         // CM6 EditorView instance (only for the active buffer)
  let loadedDirs = {};           // cache: path -> entries[]

  // ── Command palette state ─────────────────────────────────────────
  /** Get the shortcut string for the current platform from a per-platform map. */
  function getShortcut(shortcut) {
    if (typeof shortcut === 'string') return shortcut;
    if (typeof shortcut === 'object') {
      const plat = navigator.platform.includes('Mac') ? 'mac'
                 : navigator.platform.includes('Win') ? 'win' : 'linux';
      return shortcut[plat] || shortcut['default'] || '';
    }
    return '';
  }

  const paletteCommands = [
    { id: 'open-file',     name: 'Open File',          shortcut: 'mod+o',       description: 'Open a file or folder', action: () => openDialog() },
    { id: 'save',          name: 'Save',               shortcut: 'mod+s',       description: 'Save current buffer', action: () => saveCurrent() },
    { id: 'save-as',       name: 'Save As...',         shortcut: 'mod+shift+s', description: 'Save current buffer as a new file', action: () => saveAsDialog() },
    { id: 'close-tab',     name: 'Close Tab',          shortcut: {mac: 'mod+w', win: 'alt+f4', linux: 'alt+f4'}, description: 'Close the current tab', action: () => closeBuffer(buffers.active?.id) },
    { id: 'next-tab',      name: 'Next Tab',           shortcut: 'mod+tab',     description: 'Switch to the next tab', action: () => nextBuffer() },
    { id: 'prev-tab',      name: 'Previous Tab',       shortcut: 'mod+shift+tab', description: 'Switch to the previous tab', action: () => prevBuffer() },
    { id: 'toggle-sidebar',name: 'Toggle Sidebar',     shortcut: 'mod+b',       description: 'Show or hide the sidebar', action: () => toggleSidebar() },
    { id: 'show-keybinds', name: 'Show Keybinds',      shortcut: 'mod+shift+?', description: 'Show all active keyboard shortcuts', action: () => openKeybindsDialog() },
  ];
  let paletteOpen = false;

  // ── DOM refs ──────────────────────────────────────────────────────
  const fileTree       = document.getElementById('file-tree');
  const editorArea     = document.getElementById('editor-area');
  const editorContainer = document.getElementById('editor-container');
  const previewArea    = document.getElementById('preview-area');
  const previewContent = document.getElementById('preview-content');
  const emptyState     = document.getElementById('empty-state');
  const filePath       = document.getElementById('file-path');
  const tabBar         = document.getElementById('tab-bar');

  // ── Helpers ───────────────────────────────────────────────────────

  function showEditor() {
    editorArea.style.display = 'block';
    previewArea.style.display = 'none';
    emptyState.style.display = 'none';
  }

  function showPreview() {
    editorArea.style.display = 'none';
    previewArea.style.display = 'flex';
    emptyState.style.display = 'none';
  }

  function showEmpty() {
    editorArea.style.display = 'none';
    previewArea.style.display = 'none';
    emptyState.style.display = 'flex';
  }

  function escapeHtml(s) {
    const d = document.createElement('div');
    d.textContent = s;
    return d.innerHTML;
  }

  function fileNameFromPath(p) {
    const m = p.match(/[^/\\]+$/);
    return m ? m[0] : p;
  }

  function extFromPath(p) {
    const m = p.match(/\.([^.]+)$/);
    return m ? m[1].toLowerCase() : '';
  }

  function detectLanguage(path) {
    const ext = extFromPath(path);
    const map = {
      lua: 'lua', js: 'javascript', ts: 'javascript', jsx: 'javascript', tsx: 'javascript',
      css: 'css', html: 'xml', htm: 'xml', xml: 'xml',
      md: 'markdown', json: 'javascript',
      py: 'python', rb: 'python',
      c: 'clike', cpp: 'clike', h: 'clike', hpp: 'clike',
      java: 'clike', rs: 'rust', go: 'go',
      yaml: 'yaml', yml: 'yaml', toml: 'yaml',
      sh: 'shell', bash: 'shell', zsh: 'shell',
      txt: 'text', cfg: 'text', conf: 'text',
      svg: 'xml', vue: 'html', svelte: 'html',
    };
    return map[ext] || 'text';
  }

  // ── Tab bar ───────────────────────────────────────────────────────

  function renderTabs() {
    if (!tabBar) return;
    tabBar.innerHTML = '';
    const all = buffers.all;
    if (all.length === 0) {
      tabBar.style.display = 'none';
      return;
    }
    tabBar.style.display = 'flex';

    for (const buf of all) {
      const tab = document.createElement('div');
      tab.className = 'tab' + (buf.id === buffers.active?.id ? ' active' : '') + (buf.dirty ? ' dirty' : '');
      tab.dataset.bufferId = buf.id;

      // Dirty indicator dot
      if (buf.dirty) {
        const dot = document.createElement('span');
        dot.className = 'tab-dot';
        dot.textContent = '\u25CF';
        tab.appendChild(dot);
      }

      // Name
      const name = document.createElement('span');
      name.className = 'tab-name';
      name.textContent = buf.name;
      tab.appendChild(name);

      // Close button
      const close = document.createElement('span');
      close.className = 'tab-close';
      close.textContent = '\u00D7';
      close.title = 'Close';
      close.onclick = (e) => {
        e.stopPropagation();
        closeBuffer(buf.id);
      };
      tab.appendChild(close);

      // Click to switch
      tab.onclick = () => switchToBuffer(buf.id);

      tabBar.appendChild(tab);
    }
  }

  // ── Buffer switching ──────────────────────────────────────────────

  /** Save the current editor state into the active buffer, then switch. */
  function switchToBuffer(id) {
    const target = buffers.getById(id);
    if (!target) return;

    // Save current editor state into the outgoing buffer
    const current = buffers.active;
    if (current && editorView) {
      current.content = editorView.state.doc.toString();
      const sel = editorView.state.selection.main;
      current.cursorPos = { line: sel.from, col: sel.to };
    }

    // Destroy current editor
    if (editorView) {
      editorView.destroy();
      editorView = null;
    }

    // Set active
    buffers.setActive(id);
    renderTabs();

    // Render the target buffer
    renderActiveBuffer();
  }

  /** Navigate to next buffer (Ctrl+Tab or Cmd+Tab). */
  function nextBuffer() {
    const n = buffers.next();
    if (n) switchToBuffer(n.id);
  }

  /** Navigate to previous buffer (Ctrl+Shift+Tab or Cmd+Shift+Tab). */
  function prevBuffer() {
    const p = buffers.prev();
    if (p) switchToBuffer(p.id);
  }

  // ── Render active buffer ──────────────────────────────────────────

  function renderActiveBuffer() {
    const buf = buffers.active;
    if (!buf) {
      showEmpty();
      filePath.textContent = '';
      return;
    }

    filePath.textContent = buf.name + (buf.path ? ' - ' + buf.path : '') + (buf.dirty ? ' [modified]' : '');

    if (buf.type === 'image') {
      previewContent.innerHTML = '<img src="file://' + buf.path + '" alt="' + escapeHtml(buf.name) + '">';
      showPreview();
      return;
    }

    // Text: create CodeMirror editor
    editorContainer.innerHTML = '';
    editorView = window.createCoconutEditor(editorContainer, {
      content:  buf.content || '',
      language: buf.language,
      filename: buf.name,
      onSave:   () => saveBuffer(buf.id),
      onChange: (doc) => {
        // Mark dirty when content changes
            console.log("[app] buffer dirty:", buf.name);
        const text = doc.toString();
        if (text !== buf.content) {
          buf.content = text;
          if (!buf.dirty) {
            buf.dirty = true;
            filePath.textContent = buf.name + (buf.path ? ' - ' + buf.path : '') + ' [modified]';
            renderTabs();
          }
        }
      },
    });

    // Restore cursor position
    if (buf.cursorPos && editorView) {
      try {
        editorView.dispatch({
          selection: { anchor: buf.cursorPos.line, head: buf.cursorPos.col },
          scrollIntoView: true,
        });
      } catch (e) { /* ignore */ }
    }

    showEditor();
    setTimeout(() => editorView?.focus?.(), 50);
  }

  // ── Opening files ─────────────────────────────────────────────────

  function openFile(path) {
    // Highlight in tree
    document.querySelectorAll('.tree-item.selected').forEach(function (el) {
      el.classList.remove('selected');
    });
    const sel = document.querySelector('[data-path="' + escapeHtml(path) + '"]');
    if (sel) sel.classList.add('selected');

    // Check if buffer already exists for this path
    const existing = buffers.getById(
      buffers.all.find(b => b.path === path)?.id
    );
    if (existing) {
      switchToBuffer(existing.id);
      return;
    }

    console.log("[app] openFile:", path);
    // Read file from Lua backend
    console.log('[app] calling editor_read_file');
    coconut.call('editor_read_file', { path: path }).then(function (result) {
      if (result.error) {
        console.error(result.error);
        return;
      }

      const name = fileNameFromPath(path);
      const lang = detectLanguage(path);
      const type = result.type || (result.content !== undefined ? 'text' : 'unknown');

      // open buffer (creates or reuses)
      const buf = buffers.open(path, name, result.content || '', lang, type);
      buffers.setActive(buf.id);
      renderTabs();
      renderActiveBuffer();

    }).catch(function (err) {
      console.error('read_file failed:', err);
    });
  }

  // ── Saving ────────────────────────────────────────────────────────

  function saveBuffer(id) {
    const buf = buffers.getById(id);
    if (!buf) return;

    if (!buf.path) {
      // Untitled buffer — trigger Save As
      saveAsDialogFor(id);
      return;
    }

    const content = buf.content;
    console.log('[app] calling editor_save_file');
    coconut.call('editor_save_file', { path: buf.path, content: content }).then(function (result) {
      if (result.ok) {
        buf.markClean();
        filePath.textContent = buf.name + ' - ' + buf.path + ' [saved]';
        setTimeout(function () {
          if (buffers.active?.id === id) {
            filePath.textContent = buf.name + ' - ' + buf.path;
          }
        }, 2000);
        renderTabs();
      } else {
        alert('Save failed: ' + (result.error || 'unknown error'));
      }
    }).catch(function (err) {
      alert('Save error: ' + err);
    });
  }

  function saveCurrent() {
    if (!buffers.active) {
      console.warn('saveCurrent: no active buffer');
      return;
    }
    // Sync editor content into buffer
    if (editorView) {
      buffers.active.content = editorView.state.doc.toString();
    }
    saveBuffer(buffers.active.id);
  }

  // ── Close buffer ──────────────────────────────────────────────────

  function closeBuffer(id) {
    const buf = buffers.getById(id);
    if (!buf) return;

    // If dirty, confirm save
    if (buf.dirty) {
      // Simple confirm — in a real editor this would show a dialog
      const msg = buf.name + ' has unsaved changes. Close anyway?';
      if (!confirm(msg)) return;
    }

    // If this is the active buffer, destroy the editor
    if (buffers.active?.id === id && editorView) {
      editorView.destroy();
      editorView = null;
    }

    const next = buffers.close(id);
    renderTabs();
    if (next) {
      renderActiveBuffer();
    } else {
      showEmpty();
      filePath.textContent = '';
    }
  }

  // ── Open dialog ──────────────────────────────────────────────────

  function openDialog() {
    console.log('[app] calling editor_open_dialog');
    coconut.call('editor_open_dialog').then(function (result) {
      if (result.path) {
        if (result.is_dir) {
          loadDirectory(result.path);
        } else {
          openFile(result.path);
        }
      }
    });
  }

  // ── Save As dialog ───────────────────────────────────────────────

  function saveAsDialogFor(id) {
    const buf = buffers.getById(id);
    const defaultName = buf ? buf.name : 'untitled.txt';
    console.log('[app] calling editor_save_dialog');
    coconut.call('editor_save_dialog', { default_name: defaultName }).then(function (result) {
      if (result.path && buf) {
        const content = buf.content;
        console.log('[app] calling editor_save_file');
        coconut.call('editor_save_file', { path: result.path, content: content }).then(function (res) {
          if (res.ok) {
            buf.path = result.path;
            buf.name = fileNameFromPath(result.path);
            buf.language = detectLanguage(result.path);
            buf.markClean();
            filePath.textContent = buf.name + ' - saved -> ' + result.path;
            renderTabs();
          }
        });
      }
    });
  }

  function saveAsDialog() {
    if (!buffers.active) return;
    saveAsDialogFor(buffers.active.id);
  }

  // ── File tree ───────────────────────────────────────────────────────

  function renderTree(entries, container, level) {
    level = level || 0;
    for (const e of entries) {
      const item = document.createElement('div');
      item.className = 'tree-item' + (e.is_dir ? ' dir' : ' file');
      item.style.paddingLeft = (12 + level * 16) + 'px';
      item.dataset.path = e.path;
      item.dataset.name = e.name;
      item.dataset.isDir = e.is_dir;

      if (e.is_dir) {
        const icon = document.createElement('span');
        icon.className = 'tree-icon';
        icon.textContent = '\u25B6';
        item.appendChild(icon);
        item.onclick = () => toggleDir(item, e.path);
      } else {
        const spacer = document.createElement('span');
        spacer.className = 'tree-spacer';
        item.appendChild(spacer);
        item.onclick = () => openFile(e.path);
      }

      const label = document.createElement('span');
      label.textContent = e.name;
      item.appendChild(label);

      container.appendChild(item);
    }
  }

  function toggleDir(item, path) {
    const icon = item.querySelector('.tree-icon');
    if (!icon) return;
    const isOpen = icon.textContent === '\u25BC';

    if (isOpen) {
      icon.textContent = '\u25B6';
      const childrenDiv = item.parentNode.querySelector('[data-parent="' + path + '"]');
      if (childrenDiv) {
        childrenDiv.style.display = 'none';
        childrenDiv.dataset.collapsed = 'true';
      }
      return;
    }

    icon.textContent = '\u25BC';

    if (loadedDirs[path]) {
      let childrenDiv = item.parentNode.querySelector('[data-parent="' + path + '"]');
      if (childrenDiv) {
        childrenDiv.style.display = 'block';
        childrenDiv.dataset.collapsed = 'false';
        return;
      }
      const container = document.createElement('div');
      container.className = 'tree-children open';
      container.dataset.parent = path;
      container.dataset.collapsed = 'false';
      item.parentNode.insertBefore(container, item.nextSibling);
      renderTree(loadedDirs[path], container, (item.style.paddingLeft.match(/\d+/) || [12])[0] / 16 + 1);
      return;
    }

    console.log('[app] calling editor_list_dir');
    coconut.call('editor_list_dir', { path: path }).then(function (result) {
      loadedDirs[path] = result;
      const container = document.createElement('div');
      container.className = 'tree-children open';
      container.dataset.parent = path;
      container.dataset.collapsed = 'false';
      item.parentNode.insertBefore(container, item.nextSibling);
      renderTree(result, container, (item.style.paddingLeft.match(/\d+/) || [12])[0] / 16);
    }).catch(function (err) {
      icon.textContent = '\u25B6';
      console.error('list_dir failed:', err);
    });
  }

  function loadRoot() {
    console.log('[app] calling editor_list_dir');
    coconut.call('editor_list_dir', { path: '.' }).then(function (entries) {
      loadedDirs['.'] = entries;
      renderTree(entries, fileTree, 0);
    }).catch(function (err) {
      console.error('loadRoot failed:', err);
    });
  }

  function loadDirectory(path) {
    loadedDirs = {};
    fileTree.innerHTML = '';
    loadedDirs['.'] = [];
    loadedDirs[path] = [];
    console.log('[app] calling editor_list_dir');
    coconut.call('editor_list_dir', { path: path }).then(function (entries) {
      loadedDirs[path] = entries;
      renderTree(entries, fileTree, 0);
    });
    showEmpty();
  }

  // ── Sidebar toggle ──────────────────────────────────────────────

  function toggleSidebar() {
    const sidebar = document.getElementById('sidebar');
    if (sidebar) {
      const isHidden = sidebar.style.display === 'none';
      sidebar.style.display = isHidden ? '' : 'none';
    }
  }

  // ── Command palette ──────────────────────────────────────────────

  /** Build and show the command palette overlay. */
  function openCommandPalette() {
    if (paletteOpen) return;
    paletteOpen = true;

    // Create overlay
    const overlay = document.createElement('div');
    overlay.className = 'palette-overlay';
    overlay.id = 'command-palette';

    const dialog = document.createElement('div');
    dialog.className = 'palette-dialog';

    const input = document.createElement('input');
    input.type = 'text';
    input.className = 'palette-input';
    input.placeholder = 'Search commands...';
    input.spellcheck = false;
    dialog.appendChild(input);

    const results = document.createElement('div');
    results.className = 'palette-results';
    results.id = 'palette-results';
    dialog.appendChild(results);

    overlay.appendChild(dialog);
    document.body.appendChild(overlay);

    let selectedIndex = 0;

    function renderResults(query) {
      const q = (query || '').toLowerCase();
      const filtered = q
        ? paletteCommands.filter(c => c.name.toLowerCase().includes(q) || c.id.includes(q))
        : paletteCommands;
      results.innerHTML = '';
      selectedIndex = 0;

      for (let i = 0; i < filtered.length; i++) {
        const cmd = filtered[i];
        const item = document.createElement('div');
        item.className = 'palette-item' + (i === 0 ? ' selected' : '');
        item.dataset.index = i;

        const nameSpan = document.createElement('span');
        nameSpan.textContent = cmd.name;
        item.appendChild(nameSpan);

        const scText = getShortcut(cmd.shortcut);
        if (scText) {
          const sc = document.createElement('span');
          sc.className = 'shortcut';
          sc.textContent = scText;
          item.appendChild(sc);
        }

        item.onclick = () => {
          closePalette();
          cmd.action();
        };
        item.onmouseenter = () => {
          document.querySelectorAll('.palette-item.selected').forEach(el => el.classList.remove('selected'));
          item.classList.add('selected');
          selectedIndex = i;
        };

        results.appendChild(item);
      }
    }

    renderResults('');

    input.addEventListener('input', () => renderResults(input.value));

    input.addEventListener('keydown', (e) => {
      const items = results.querySelectorAll('.palette-item');
      if (e.key === 'Escape') {
        closePalette();
        e.preventDefault();
        return;
      }
      if (e.key === 'ArrowDown') {
        e.preventDefault();
        if (items.length === 0) return;
        selectedIndex = Math.min(selectedIndex + 1, items.length - 1);
        updateSelection();
        return;
      }
      if (e.key === 'ArrowUp') {
        e.preventDefault();
        if (items.length === 0) return;
        selectedIndex = Math.max(selectedIndex - 1, 0);
        updateSelection();
        return;
      }
      if (e.key === 'Enter') {
        e.preventDefault();
        const filtered = getFilteredCommands();
        if (filtered[selectedIndex]) {
          closePalette();
          filtered[selectedIndex].action();
        }
        return;
      }
    });

    function updateSelection() {
      const items = results.querySelectorAll('.palette-item');
      items.forEach((el, i) => {
        el.classList.toggle('selected', i === selectedIndex);
        if (i === selectedIndex) el.scrollIntoView({ block: 'nearest' });
      });
    }

    function getFilteredCommands() {
      const q = (input.value || '').toLowerCase();
      return q
        ? paletteCommands.filter(c => c.name.toLowerCase().includes(q) || c.id.includes(q))
        : paletteCommands;
    }

    // Close on overlay click (outside dialog)
    overlay.addEventListener('click', (e) => {
      if (e.target === overlay) closePalette();
    });

    setTimeout(() => input.focus(), 50);
  }

  function closePalette() {
    paletteOpen = false;
    const overlay = document.getElementById('command-palette');
    if (overlay) overlay.remove();
  }

  // ── Keybinds dialog ───────────────────────────────────────────────

  let keybindsDialogOpen = false;

  function openKeybindsDialog() {
    if (keybindsDialogOpen) return;
    keybindsDialogOpen = true;

    const overlay = document.createElement('div');
    overlay.className = 'palette-overlay';
    overlay.id = 'keybinds-dialog';

    const dialog = document.createElement('div');
    dialog.className = 'palette-dialog';
    dialog.style.width = '500px';

    const header = document.createElement('div');
    header.className = 'palette-header';
    header.textContent = 'Keyboard Shortcuts';
    dialog.appendChild(header);

    const results = document.createElement('div');
    results.className = 'palette-results';
    dialog.appendChild(results);

    const keybinds = coconut.getKeybinds ? coconut.getKeybinds() : [];
    for (const kb of keybinds) {
      const item = document.createElement('div');
      item.className = 'palette-item';

      const nameSpan = document.createElement('span');
      nameSpan.textContent = kb.description || kb.id;
      item.appendChild(nameSpan);

      const sc = document.createElement('span');
      sc.className = 'shortcut';
      sc.textContent = kb.combo;
      item.appendChild(sc);

      results.appendChild(item);
    }

    overlay.appendChild(dialog);
    document.body.appendChild(overlay);

    overlay.addEventListener('click', (e) => {
      if (e.target === overlay) closeKeybindsDialog();
    });
    document.addEventListener('keydown', function escHandler(e) {
      if (e.key === 'Escape') {
        closeKeybindsDialog();
        document.removeEventListener('keydown', escHandler);
      }
    });
  }

  function closeKeybindsDialog() {
    keybindsDialogOpen = false;
    const overlay = document.getElementById('keybinds-dialog');
    if (overlay) overlay.remove();
  }

  // ── Init ──────────────────────────────────────────────────────────

  document.addEventListener('DOMContentLoaded', function () {
    loadRoot();
    showEmpty();

    // Legacy keyboard shortcuts (before coconut.keybind is ready)
    document.addEventListener('keydown', function (e) {
      const mod = e.metaKey || e.ctrlKey;
      if (mod && e.key === 'Tab' && !e.shiftKey) {
        e.preventDefault();
        nextBuffer();
      }
      if (mod && e.key === 'Tab' && e.shiftKey) {
        e.preventDefault();
        prevBuffer();
      }
    });

    // ── Keybinds via coconut.keybind ───────────────────────────
    function tryRegisterKeybinds() {
      if (typeof coconut.keybind !== 'function') {
        setTimeout(tryRegisterKeybinds, 100);
        return;
      }

      // Command palette
      coconut.keybind('mod+shift+p', function () {
        openCommandPalette();
      }, { id: 'editor.palette', scope: 'editor', description: 'Open command palette' });

      // Save
      coconut.keybind('mod+s', function () {
        saveCurrent();
      }, { id: 'editor.save', scope: 'editor', description: 'Save current file' });

      // Open
      coconut.keybind('mod+o', function () {
        openDialog();
      }, { id: 'editor.open', scope: 'editor', description: 'Open file or folder' });

      // Close tab (mod+w on macOS, alt+f4 on Windows/Linux)
      coconut.keybind({mac: 'mod+w', win: 'alt+f4', linux: 'alt+f4'}, function () {
        closeBuffer(buffers.active?.id);
      }, { id: 'editor.close-tab', scope: 'editor', description: 'Close the current tab' });

      // Tab switching
      coconut.keybind('mod+tab', function () {
        nextBuffer();
      }, { id: 'editor.next-tab', scope: 'editor', description: 'Next tab' });

      coconut.keybind('mod+shift+tab', function () {
        prevBuffer();
      }, { id: 'editor.prev-tab', scope: 'editor', description: 'Previous tab' });

      // Toggle sidebar
      coconut.keybind('mod+b', function () {
        toggleSidebar();
      }, { id: 'editor.toggle-sidebar', scope: 'editor', description: 'Toggle sidebar' });

      // Show keybinds dialog
      coconut.keybind('mod+shift+?', function () {
        openKeybindsDialog();
      }, { id: 'editor.show-keybinds', scope: 'editor', description: 'Show all keyboard shortcuts' });

      console.log('[app] keybinds registered');
    }
    tryRegisterKeybinds();

    // ── CLI args: open file/folder on launch ────────────────────
    function checkCliArgs() {
      const args = coconut.args || window.__coconut_args;
      if (args && args.positional && args.positional.length > 0) {
        const path = args.positional[0];
        console.log('[app] CLI arg detected:', path);
        // Open the file or directory
        if (path) {
          // Check if it's a directory by listing it
          coconut.call('editor_list_dir', { path: path }).then(function (entries) {
            // Is a directory
            loadDirectory(path);
          }).catch(function () {
            // Not a directory — open as file
            openFile(path);
          });
        }
      }
    }
    // Wait a bit for bridge to be ready
    setTimeout(checkCliArgs, 200);
  });

  window.openFile = openFile;
  window.saveCurrent = saveCurrent;
  window.openDialog = openDialog;
  window.saveAsDialog = saveAsDialog;
  window.openCommandPalette = openCommandPalette;
  window.closePalette = closePalette;
  window.openKeybindsDialog = openKeybindsDialog;

})();
