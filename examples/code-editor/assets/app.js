// Coconut Code Editor — client-side logic
(function () {
  'use strict';

  // ── State ─────────────────────────────────────────────────────────
  let currentFile = null;       // { path, name, type, text_type? }
  let editor = null;            // CodeMirror instance
  let loadedDirs = {};          // cache: path -> entries[]

  // ── DOM refs ──────────────────────────────────────────────────────
  const fileTree = document.getElementById('file-tree');
  const editorArea = document.getElementById('editor-area');
  const previewArea = document.getElementById('preview-area');
  const previewContent = document.getElementById('preview-content');
  const emptyState = document.getElementById('empty-state');
  const filePath = document.getElementById('file-path');
  const textarea = document.getElementById('editor-textarea');

  // ── Helpers ───────────────────────────────────────────────────────

  function showEditor() {
    editorArea.style.display = 'block';
    previewArea.style.display = 'none';
    emptyState.style.display = 'none';
    if (editor) setTimeout(() => editor.refresh(), 50);
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

  // ── File Tree ─────────────────────────────────────────────────────

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
    const isOpen = icon.textContent === '\u25BC';

    if (isOpen) {
      // Collapse
      icon.textContent = '\u25B6';
      let next = item.nextSibling;
      while (next && next.dataset && next.dataset.path) {
        const sibling = next;
        next = sibling.nextSibling;
        if (sibling.dataset.path && sibling.dataset.path.startsWith(path + '/') ||
            sibling.dataset.path && path.startsWith(sibling.dataset.path)) {
          // Actually, just remove all siblings that are children
        } else {
          break;
        }
      }
      // Simpler: find the children container
      let childrenDiv = item.parentNode.querySelector('[data-parent="' + path + '"]');
      if (childrenDiv) {
        childrenDiv.style.display = 'none';
        childrenDiv.dataset.collapsed = 'true';
      }
      return;
    }

    icon.textContent = '\u25BC';

    // Check cache
    if (loadedDirs[path]) {
      // Already loaded — show cached children
      let childrenDiv = item.parentNode.querySelector('[data-parent="' + path + '"]');
      if (childrenDiv) {
        childrenDiv.style.display = 'block';
        childrenDiv.dataset.collapsed = 'false';
        return;
      }
      // Create new children container
      const container = document.createElement('div');
      container.className = 'tree-children open';
      container.dataset.parent = path;
      container.dataset.collapsed = 'false';
      item.parentNode.insertBefore(container, item.nextSibling);
      renderTree(loadedDirs[path], container, (item.style.paddingLeft.match(/\d+/) || [12])[0] / 16 + 1);
      return;
    }

    // Load from backend
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
    coconut.call('editor_list_dir', { path: '.' }).then(function (entries) {
      loadedDirs['.'] = entries;
      renderTree(entries, fileTree, 0);
    }).catch(function (err) {
      console.error('loadRoot failed:', err);
    });
  }

  // ── Opening files ─────────────────────────────────────────────────

  function openFile(path) {
    // Highlight selected
    document.querySelectorAll('.tree-item.selected').forEach(function (el) {
      el.classList.remove('selected');
    });
    document.querySelector('[data-path="' + escapeHtml(path) + '"]')?.classList.add('selected');

    coconut.call('editor_read_file', { path: path }).then(function (result) {
      if (result.error) {
        console.error(result.error);
        return;
      }

      currentFile = result;
      filePath.textContent = result.name + ' — ' + result.path;

      if (result.type === 'image') {
        // Image preview via file:// URL
        previewContent.innerHTML = '<img src="file://' + result.path + '" alt="' + escapeHtml(result.name) + '">';
        showPreview();
      } else if (result.type === 'text') {
        showEditor();
        if (!editor) {
          editor = CodeMirror.fromTextArea(textarea, {
            lineNumbers: true,
            theme: 'dracula',
            indentUnit: 2,
            tabSize: 2,
            autoCloseBrackets: true,
            styleActiveLine: true,
            viewportMargin: Infinity,
            extraKeys: {
              'Ctrl-S': function () { saveCurrent(); },
              'Cmd-S': function () { saveCurrent(); },
            },
          });
        }
        editor.setValue(result.content || '');
        editor.setOption('mode', result.text_type || 'text');
        editor.clearHistory();
        editor.focus();
      }
    }).catch(function (err) {
      console.error('read_file failed:', err);
    });
  }

  // ── Saving ────────────────────────────────────────────────────────

  function saveCurrent() {
    if (!currentFile || !editor) {
      console.warn('Nothing to save');
      return;
    }
    const content = editor.getValue();
    coconut.call('editor_save_file', { path: currentFile.path, content: content }).then(function (result) {
      if (result.ok) {
        filePath.textContent = currentFile.name + ' — saved ✓';
        setTimeout(function () {
          if (currentFile) filePath.textContent = currentFile.name + ' — ' + currentFile.path;
        }, 2000);
      } else {
        alert('Save failed: ' + (result.error || 'unknown error'));
      }
    }).catch(function (err) {
      alert('Save error: ' + err);
    });
  }

  function openDialog() {
    coconut.call('editor_open_dialog').then(function (result) {
      if (result.path) {
        openFile(result.path);
      }
    });
  }

  function saveAsDialog() {
    const defaultName = currentFile ? currentFile.name : 'untitled.txt';
    coconut.call('editor_save_dialog', { default_name: defaultName }).then(function (result) {
      if (result.path) {
        // Read current content and save to new path
        const content = editor ? editor.getValue() : '';
        coconut.call('editor_save_file', { path: result.path, content: content }).then(function (res) {
          if (res.ok) {
            filePath.textContent = 'saved → ' + result.path;
          }
        });
      }
    });
  }

  // ── Init ──────────────────────────────────────────────────────────

  document.addEventListener('DOMContentLoaded', function () {
    loadRoot();
    showEmpty();
  });

  // Expose for toolbar onclick
  window.openFile = openFile;
  window.saveCurrent = saveCurrent;
  window.openDialog = openDialog;
  window.saveAsDialog = saveAsDialog;

})();
