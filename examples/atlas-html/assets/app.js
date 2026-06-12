// Atlas Tool — adapted from atlas-packer.js for Coconut Milk
// Sprite Atlas Packer + Tileset Resizer

/* ─── Toast ─── */
function toast(msg) {
  const t = document.getElementById('toast');
  t.textContent = msg;
  t.classList.add('show');
  setTimeout(() => t.classList.remove('show'), 1500);
}

/* ─── Project persistence via Lua bridge ─── */
async function bridgeSave(name, data) {
  try {
    const r = await coconut.call('project_save', { name: name, data: JSON.stringify(data) });
    return r.ok;
  } catch (e) { return false; }
}

async function bridgeLoad(name) {
  try {
    const r = await coconut.call('project_load', { name: name });
    if (r.ok && r.data) return JSON.parse(r.data);
  } catch (e) {}
  return null;
}

async function bridgeList() {
  try {
    const r = await coconut.call('project_list', {});
    return r.projects || [];
  } catch (e) { return []; }
}

async function bridgeDelete(name) {
  try {
    const r = await coconut.call('project_delete', { name: name });
    return r.ok;
  } catch (e) { return false; }
}

/* ─── Alpine project manager ─── */
function projectManager() {
  return {
    showSave: false,
    showLoad: false,
    saveName: '',
    list: [],
    currentName() {
      const raw = localStorage.getItem('atlas-project-current');
      return raw ? raw : 'unsaved';
    },
    async refreshList() {
      const names = await bridgeList();
      this.list = names.map(n => ({ name: n, date: '' }));
    },
    async saveCurrent() {
      const current = localStorage.getItem('atlas-project-current');
      if (current && current !== 'unsaved') {
        await saveProjectAs(current);
        toast('saved "' + current + '"');
      } else {
        this.showSave = true;
      }
    },
    async doSave() {
      const name = this.saveName.trim();
      if (!name) return;
      await saveProjectAs(name);
      localStorage.setItem('atlas-project-current', name);
      this.showSave = false;
      toast('saved "' + name + '"');
    },
    async doLoad(name) {
      await loadProject(name);
      localStorage.setItem('atlas-project-current', name);
      this.showLoad = false;
    },
    async doDelete(i, name) {
      const ok = await bridgeDelete(name);
      if (ok) {
        localStorage.removeItem('atlas-project-' + name);
        const names = await bridgeList();
        this.list = names.map(n => ({ name: n, date: '' }));
        toast('deleted "' + name + '"');
      }
    },
    newProject() {
      if (sprites.length > 0 && !confirm('Clear current project?')) return;
      clearAll();
      localStorage.removeItem('atlas-project-current');
      this.showSave = false;
      this.showLoad = false;
    }
  };
}

async function saveProjectAs(name) {
  const data = {
    sprites: sprites.map(s => ({ name: s.name, dataUrl: s.img.src, w: s.w, h: s.h })),
    cols: colsInput.value,
    padding: paddingInput.value,
    fitMode: fitMode.value,
    keepOriginalSize: keepOriginalSize.checked,
    transparentBg: transparentBg.checked,
    bgColor: bgColor.value,
    exportName: document.getElementById('exportName').value
  };
  const ok = await bridgeSave(name, data);
  if (!ok) toast('save failed');
}

async function loadProject(name) {
  const data = await bridgeLoad(name);
  if (!data) { toast('project not found'); return; }
  clearAll();
  if (data.sprites && data.sprites.length) {
    let loaded = 0;
    data.sprites.forEach(({ name, dataUrl, w, h }) => {
      const img = new Image();
      img.onload = () => {
        sprites.push({ name, img, w, h });
        loaded++;
        if (loaded === data.sprites.length) {
          renderSpriteList();
          packBtn.disabled = false;
        }
      };
      img.src = dataUrl;
    });
  }
  if (data.cols) colsInput.value = data.cols;
  if (data.padding) paddingInput.value = data.padding;
  if (data.fitMode) fitMode.value = data.fitMode;
  if (data.keepOriginalSize !== undefined) keepOriginalSize.checked = data.keepOriginalSize;
  if (data.transparentBg !== undefined) transparentBg.checked = data.transparentBg;
  if (data.bgColor) bgColor.value = data.bgColor;
  if (data.exportName) document.getElementById('exportName').value = data.exportName;
  bgColor.disabled = transparentBg.checked;
  toast('loaded "' + name + '"');
}

function clearAll() {
  sprites.length = 0;
  atlasData = null;
  renderSpriteList();
  packBtn.disabled = true;
  metaCode.textContent = '';
  metaOutput.style.display = 'none';
  imagePane.querySelector('span').style.display = '';
  atlasCanvas.style.display = 'none';
  downloadPngBtn.disabled = true;
  downloadMetaBtn.disabled = true;
  downloadAllBtn.disabled = true;
  atlasInfo.style.display = 'none';
  clearCache();
  saveSprites();
}

/* ─── DOM refs ─── */
const fileInput = document.getElementById('fileInput');
const zipInput = document.getElementById('zipInput');
const importBtn = document.getElementById('importBtn');
const importZipBtn = document.getElementById('importZipBtn');
const spriteList = document.getElementById('spriteList');
const spriteCount = document.getElementById('spriteCount');
const colsInput = document.getElementById('colsInput');
const paddingInput = document.getElementById('paddingInput');
const transparentBg = document.getElementById('transparentBg');
const bgColor = document.getElementById('bgColor');
const fitMode = document.getElementById('fitMode');
const keepOriginalSize = document.getElementById('keepOriginalSize');
const packBtn = document.getElementById('packBtn');
const atlasCanvas = document.getElementById('atlasCanvas');
const imagePane = document.getElementById('imagePane');
const metaPane = document.getElementById('metaPane');
const metaOutput = document.getElementById('metaOutput');
const metaCode = document.getElementById('metaCode');
const downloadPngBtn = document.getElementById('downloadPngBtn');
const downloadMetaBtn = document.getElementById('downloadMetaBtn');
const downloadAllBtn = document.getElementById('downloadAllBtn');
const exportNameInput = document.getElementById('exportName');
const atlasInfo = document.getElementById('atlasInfo');
const infoCanvas = document.getElementById('infoCanvas');
const infoCell = document.getElementById('infoCell');

const sprites = [];
let atlasData = null;
let dragSrcIdx = null;

const STORAGE_KEY = 'atlas-packer';
const CACHE_KEY = 'atlas-packer-cache';

function saveCache() {
  try {
    const info = atlasInfo.style.display !== 'none' ? {
      canvasSize: infoCanvas.textContent,
      cellSize: infoCell.textContent
    } : null;
    localStorage.setItem(CACHE_KEY, JSON.stringify({
      png: atlasCanvas.toDataURL('image/png'),
      json: metaCode.textContent,
      info
    }));
  } catch(e) {}
}

function loadCache() {
  try {
    const raw = localStorage.getItem(CACHE_KEY);
    if (!raw) return;
    const data = JSON.parse(raw);
    if (!data.png || !data.json) return;
    const img = new Image();
    img.onload = () => {
      atlasCanvas.width = img.naturalWidth;
      atlasCanvas.height = img.naturalHeight;
      const ctx = atlasCanvas.getContext('2d');
      ctx.drawImage(img, 0, 0);
      atlasCanvas.style.display = '';
      imagePane.querySelector('span').style.display = 'none';
      atlasZoom.center();
      metaCode.textContent = data.json;
      metaOutput.style.display = '';
      metaPane.querySelector('.empty-state').style.display = 'none';
      Prism.highlightElement(metaCode);
      atlasData = JSON.parse(data.json);
      if (data.info) {
        infoCanvas.textContent = data.info.canvasSize;
        infoCell.textContent = data.info.cellSize;
        atlasInfo.style.display = 'flex';
      }
      downloadPngBtn.disabled = false;
      downloadMetaBtn.disabled = false;
      downloadAllBtn.disabled = false;
    };
    img.src = data.png;
  } catch(e) {}
}

function clearCache() {
  try { localStorage.removeItem(CACHE_KEY); } catch(e) {}
}

function saveSprites() {
  try {
    localStorage.setItem(STORAGE_KEY, JSON.stringify({
      sprites: sprites.map(s => ({ name: s.name, dataUrl: s.img.src, w: s.w, h: s.h })),
      cols: colsInput.value,
      padding: paddingInput.value,
      fitMode: fitMode.value,
      keepOriginalSize: keepOriginalSize.checked,
      transparentBg: transparentBg.checked,
      bgColor: bgColor.value,
      exportName: exportNameInput.value
    }));
  } catch (e) {}
}

function loadSprites() {
  try {
    const raw = localStorage.getItem(STORAGE_KEY);
    if (!raw) return;
    const data = JSON.parse(raw);
    if (data.sprites && data.sprites.length) {
      let loaded = 0;
      data.sprites.forEach(({ name, dataUrl, w, h }) => {
        const img = new Image();
        img.onload = () => {
          sprites.push({ name, img, w, h });
          loaded++;
          if (loaded === data.sprites.length) {
            renderSpriteList();
            packBtn.disabled = false;
          }
        };
        img.src = dataUrl;
      });
    }
    if (data.cols) colsInput.value = data.cols;
    if (data.padding) paddingInput.value = data.padding;
    if (data.fitMode) fitMode.value = data.fitMode;
    if (data.keepOriginalSize !== undefined) keepOriginalSize.checked = data.keepOriginalSize;
    if (data.transparentBg !== undefined) transparentBg.checked = data.transparentBg;
    if (data.bgColor) bgColor.value = data.bgColor;
    if (data.exportName) exportNameInput.value = data.exportName;
    bgColor.disabled = transparentBg.checked;
  } catch (e) {}
}

/* ─── Init ─── */
loadSprites();
loadCache();

transparentBg.addEventListener('change', () => { bgColor.disabled = transparentBg.checked; });

/* ─── Tabs ─── */
document.querySelectorAll('.tab').forEach(tab => {
  tab.addEventListener('click', () => {
    document.querySelectorAll('.tab').forEach(t => t.classList.remove('active'));
    tab.classList.add('active');
    const show = tab.dataset.tab;
    imagePane.style.display = show === 'image' ? 'flex' : 'none';
    metaPane.style.display = show === 'meta' ? 'block' : 'none';
    if (show === 'meta' && metaCode.textContent) {
      Prism.highlightElement(metaCode);
    }
  });
});

/* ─── Import dropdown ─── */
importBtn.addEventListener('click', () => fileInput.click());
importZipBtn.addEventListener('click', () => zipInput.click());

/* ─── File handling ─── */
fileInput.addEventListener('change', () => { handleFiles(fileInput.files); fileInput.value = ''; });
zipInput.addEventListener('change', async () => {
  if (!zipInput.files.length) return;
  const zipFile = zipInput.files[0];
  try {
    const data = await zipFile.arrayBuffer();
    const zip = await JSZip.loadAsync(data);
    const entries = [];
    zip.forEach((path, entry) => {
      if (!entry.dir && /\.(png|gif|webp|jpg|jpeg)$/i.test(path)) {
        entries.push({ path, entry });
      }
    });
    for (const { path, entry } of entries) {
      const blob = await entry.async('blob');
      const name = path.replace(/\.(png|gif|webp|jpg|jpeg)$/i, '').replace(/.*[/\\]/, '');
      const dataUrl = await new Promise(resolve => {
        const r = new FileReader();
        r.onload = () => resolve(r.result);
        r.readAsDataURL(blob);
      });
      const img = new Image();
      await new Promise((resolve, reject) => {
        img.onload = resolve;
        img.onerror = reject;
        img.src = dataUrl;
      });
      sprites.push({ name, img, w: img.naturalWidth, h: img.naturalHeight });
    }
    renderSpriteList();
    packBtn.disabled = false;
    saveSprites();
    clearCache();
    toast('imported ' + entries.length + ' images from zip');
  } catch (e) {
    toast('failed to read zip');
    console.error(e);
  }
  zipInput.value = '';
});

function handleFiles(files) {
  for (const f of files) {
    if (!f.type.startsWith('image/')) continue;
    const reader = new FileReader();
    reader.onload = e => {
      const img = new Image();
      img.onload = () => {
        sprites.push({ name: f.name.replace(/\.(png|gif|webp|jpg|jpeg)$/i, ''), img, w: img.naturalWidth, h: img.naturalHeight });
        renderSpriteList();
        packBtn.disabled = false;
        saveSprites();
        clearCache();
      };
      img.src = e.target.result;
    };
    reader.readAsDataURL(f);
  }
}

/* ─── Sprite list with drag reorder ─── */
function handleDragStart(e, idx) {
  dragSrcIdx = idx;
  e.dataTransfer.effectAllowed = 'move';
  e.dataTransfer.setData('text/plain', idx);
  e.currentTarget.classList.add('dragging');
}

function handleDragOver(e, idx) {
  e.preventDefault();
  e.dataTransfer.dropEffect = 'move';
  document.querySelectorAll('.sprite-item').forEach(el => el.classList.remove('drag-over'));
  if (dragSrcIdx !== null && dragSrcIdx !== idx) {
    e.currentTarget.classList.add('drag-over');
  }
}

function handleDragLeave(e) {
  e.currentTarget.classList.remove('drag-over');
}

function handleDrop(e, idx) {
  e.preventDefault();
  e.currentTarget.classList.remove('drag-over');
  if (dragSrcIdx === null || dragSrcIdx === idx) { dragSrcIdx = null; return; }
  const item = sprites.splice(dragSrcIdx, 1)[0];
  sprites.splice(idx, 0, item);
  dragSrcIdx = null;
  renderSpriteList();
  saveSprites();
  clearCache();
}

function handleDragEnd(e) {
  e.currentTarget.classList.remove('dragging');
  document.querySelectorAll('.sprite-item').forEach(el => el.classList.remove('drag-over'));
  dragSrcIdx = null;
}

function renderSpriteList() {
  spriteList.innerHTML = '';
  spriteCount.textContent = sprites.length;
  sprites.forEach((s, i) => {
    const div = document.createElement('div');
    div.className = 'sprite-item';
    div.draggable = true;
    div.innerHTML = '<span class="drag-handle">⠿</span>' +
      '<img src="' + s.img.src + '" alt="' + s.name + '">' +
      '<span class="name">' + s.name + '</span>' +
      '<span class="size">' + s.w + '&times;' + s.h + '</span>' +
      '<span class="remove" data-idx="' + i + '">&times;</span>';
    div.addEventListener('dragstart', e => handleDragStart(e, i));
    div.addEventListener('dragover', e => handleDragOver(e, i));
    div.addEventListener('dragleave', handleDragLeave);
    div.addEventListener('drop', e => handleDrop(e, i));
    div.addEventListener('dragend', handleDragEnd);
    div.querySelector('.remove').addEventListener('click', () => {
      sprites.splice(i, 1);
      renderSpriteList();
      packBtn.disabled = sprites.length === 0;
      saveSprites();
      clearCache();
      if (sprites.length === 0) {
        atlasData = null;
        metaCode.textContent = '';
        metaOutput.style.display = 'none';
        imagePane.querySelector('span').style.display = '';
        atlasCanvas.style.display = 'none';
        atlasInfo.style.display = 'none';
        downloadPngBtn.disabled = true;
        downloadMetaBtn.disabled = true;
        downloadAllBtn.disabled = true;
      }
    });
    spriteList.appendChild(div);
  });
}

/* ─── Pack atlas ─── */
function packAtlas() {
  if (sprites.length === 0) { toast('no sprites'); return; }
  const cols = parseInt(colsInput.value) || 8;
  const pad = parseInt(paddingInput.value) || 2;
  const mode = fitMode.value;

  const rows = Math.ceil(sprites.length / cols);
  const cellW = Math.max(...sprites.map(s => s.w));
  const cellH = Math.max(...sprites.map(s => s.h));

  const canvasW = cols * (cellW + pad) + pad;
  const canvasH = rows * (cellH + pad) + pad;

  atlasCanvas.width = canvasW;
  atlasCanvas.height = canvasH;
  atlasCanvas.style.display = '';
  imagePane.querySelector('span').style.display = 'none';
  atlasZoom.center();
  const ctx = atlasCanvas.getContext('2d');

  if (transparentBg.checked) {
    ctx.clearRect(0, 0, canvasW, canvasH);
  } else {
    ctx.fillStyle = bgColor.value;
    ctx.fillRect(0, 0, canvasW, canvasH);
  }

  const frames = {};

  sprites.forEach((s, i) => {
    const col = i % cols;
    const row = Math.floor(i / cols);
    const x = pad + col * (cellW + pad);
    const y = pad + row * (cellH + pad);

    if (keepOriginalSize.checked) {
      const dx = x + (cellW - s.w) / 2;
      const dy = y + (cellH - s.h) / 2;
      ctx.drawImage(s.img, dx, dy, s.w, s.h);
    } else if (mode === 'contain') {
      const scale = Math.min(cellW / s.w, cellH / s.h);
      const dw = s.w * scale;
      const dh = s.h * scale;
      const dx = x + (cellW - dw) / 2;
      const dy = y + (cellH - dh) / 2;
      ctx.drawImage(s.img, dx, dy, dw, dh);
    } else if (mode === 'cover') {
      const scale = Math.max(cellW / s.w, cellH / s.h);
      const dw = s.w * scale;
      const dh = s.h * scale;
      const dx = x + (cellW - dw) / 2;
      const dy = y + (cellH - dh) / 2;
      ctx.drawImage(s.img, dx, dy, dw, dh);
    } else {
      ctx.drawImage(s.img, x, y, cellW, cellH);
    }

    frames[s.name] = {
      frame: { x: Math.round(x), y: Math.round(y), w: cellW, h: cellH },
      spriteSourceSize: { x: 0, y: 0, w: s.w, h: s.h },
      sourceSize: { w: s.w, h: s.h },
      rotated: false,
      trimmed: false
    };
  });

  atlasData = { frames: frames };

  metaCode.textContent = JSON.stringify(atlasData, null, 2);
  metaOutput.style.display = '';
  metaPane.querySelector('.empty-state').style.display = 'none';
  Prism.highlightElement(metaCode);
  downloadPngBtn.disabled = false;
  downloadMetaBtn.disabled = false;
  downloadAllBtn.disabled = false;
  saveSprites();
  saveCache();
  infoCanvas.textContent = canvasW + ' x ' + canvasH;
  infoCell.textContent = keepOriginalSize.checked ? 'cell: variable' : 'cell: ' + cellW + ' x ' + cellH;
  atlasInfo.style.display = 'flex';
  toast('packed ' + sprites.length + ' sprites (' + canvasW + 'x' + canvasH + ')');
}

function exportBase() { return exportNameInput.value.trim() || 'atlas'; }

function downloadPng() {
  if (!atlasData) return;
  atlasCanvas.toBlob(blob => {
    const a = document.createElement('a');
    a.href = URL.createObjectURL(blob);
    a.download = exportBase() + '.png';
    a.click();
    URL.revokeObjectURL(a.href);
  });
}

function downloadMeta() {
  if (!atlasData) return;
  const a = document.createElement('a');
  a.href = URL.createObjectURL(new Blob([metaCode.textContent], { type: 'application/json' }));
  a.download = exportBase() + '.json';
  a.click();
  URL.revokeObjectURL(a.href);
}

async function downloadAll() {
  if (!atlasData) return;
  const base = exportBase();
  const ZIP = new JSZip();
  const pngBlob = await new Promise(resolve => atlasCanvas.toBlob(resolve));
  ZIP.file(base + '.png', pngBlob);
  ZIP.file(base + '.json', metaCode.textContent);
  const content = await ZIP.generateAsync({ type: 'blob' });
  const a = document.createElement('a');
  a.href = URL.createObjectURL(content);
  a.download = base + '.zip';
  a.click();
  URL.revokeObjectURL(a.href);
}

packBtn.addEventListener('click', packAtlas);
downloadPngBtn.addEventListener('click', downloadPng);
downloadMetaBtn.addEventListener('click', downloadMeta);
downloadAllBtn.addEventListener('click', downloadAll);

/* ─── Zoom/Pan ─── */
function initZoomPan(wrapper, canvas, resetBtn) {
  let scale = 1;
  let panX = 0;
  let panY = 0;
  let isDragging = false;
  let lastX = 0;
  let lastY = 0;

  function updateTransform() {
    canvas.style.transform = 'translate(' + panX + 'px, ' + panY + 'px) scale(' + scale + ')';
  }

  function center() {
    panX = (wrapper.clientWidth - canvas.width) / 2;
    panY = (wrapper.clientHeight - canvas.height) / 2;
    scale = 1;
    updateTransform();
  }

  wrapper.addEventListener('wheel', function(e) {
    e.preventDefault();
    var rect = wrapper.getBoundingClientRect();
    var mx = e.clientX - rect.left;
    var my = e.clientY - rect.top;
    var zoomFactor = e.deltaY < 0 ? 1.15 : 0.87;
    var newScale = Math.max(0.1, Math.min(10, scale * zoomFactor));
    panX = mx - (mx - panX) * (newScale / scale);
    panY = my - (my - panY) * (newScale / scale);
    scale = newScale;
    updateTransform();
  }, { passive: false });

  wrapper.addEventListener('mousedown', function(e) {
    if (e.target !== canvas && e.target !== wrapper) return;
    isDragging = true;
    lastX = e.clientX;
    lastY = e.clientY;
    wrapper.style.cursor = 'grabbing';
  });

  window.addEventListener('mousemove', function(e) {
    if (!isDragging) return;
    var dx = e.clientX - lastX;
    var dy = e.clientY - lastY;
    panX += dx;
    panY += dy;
    lastX = e.clientX;
    lastY = e.clientY;
    updateTransform();
  });

  window.addEventListener('mouseup', function() {
    isDragging = false;
    wrapper.style.cursor = 'grab';
  });

  if (resetBtn) resetBtn.addEventListener('click', center);
  return { center: center };
}

/* ─── Resize Handles ─── */
function initResizeHandle(handle, prevEl, nextEl, direction, minSize) {
  let start, startPrev, startNext;
  const isHoriz = direction === 'horizontal';

  handle.addEventListener('mousedown', function(e) {
    e.preventDefault();
    start = isHoriz ? e.clientX : e.clientY;
    var prevRect = prevEl.getBoundingClientRect();
    var nextRect = nextEl ? nextEl.getBoundingClientRect() : null;
    startPrev = isHoriz ? prevRect.width : prevRect.height;
    startNext = nextRect ? (isHoriz ? nextRect.width : nextRect.height) : 0;

    prevEl.style.flex = '0 0 auto';
    if (nextEl) nextEl.style.flex = '0 0 auto';

    var onMove = function(e2) {
      var current = isHoriz ? e2.clientX : e2.clientY;
      var delta = current - start;
      var newPrev = Math.max(minSize, startPrev + delta);
      if (isHoriz) {
        prevEl.style.width = newPrev + 'px';
        if (nextEl) nextEl.style.width = Math.max(minSize, startNext - delta) + 'px';
      } else {
        prevEl.style.height = newPrev + 'px';
        if (nextEl) nextEl.style.height = Math.max(minSize, startNext - delta) + 'px';
      }
    };

    var onUp = function() {
      document.removeEventListener('mousemove', onMove);
      document.removeEventListener('mouseup', onUp);
    };

    document.addEventListener('mousemove', onMove);
    document.addEventListener('mouseup', onUp);
  });
}

/* ─── Nav Switching ─── */
document.querySelectorAll('.nav-item').forEach(btn => {
  btn.addEventListener('click', () => {
    document.querySelectorAll('.nav-item').forEach(b => b.classList.remove('active'));
    btn.classList.add('active');
    document.querySelectorAll('.tool-page').forEach(p => p.classList.remove('active'));
    document.getElementById(btn.dataset.tool + 'Page').classList.add('active');
  });
});

/* ─── Atlas Zoom/Pan ─── */
const atlasZoom = initZoomPan(imagePane, atlasCanvas, document.getElementById('resetAtlasZoom'));

/* ─── Atlas Resizers ─── */
initResizeHandle(document.getElementById('atlasLeftHandle'), document.getElementById('atlasLeftSidebar'), document.getElementById('atlasMain'), 'horizontal', 160);
initResizeHandle(document.getElementById('atlasRightHandle'), document.getElementById('atlasMain'), document.getElementById('atlasRightSidebar'), 'horizontal', 160);
initResizeHandle(document.getElementById('atlasSettingsHandle'), document.getElementById('atlasSettingsSection'), document.getElementById('atlasExportSection'), 'vertical', 60);
initResizeHandle(document.getElementById('atlasImportHandle'), document.getElementById('atlasImportSection'), document.getElementById('atlasSpritesSection'), 'vertical', 60);

/* ─── Tileset Resizer ─── */
const tilesetFileInput = document.getElementById('tilesetFileInput');
const tilesetImportBtn = document.getElementById('tilesetImportBtn');
const origTileW = document.getElementById('origTileW');
const origTileH = document.getElementById('origTileH');
const origSpacing = document.getElementById('origSpacing');
const origMargin = document.getElementById('origMargin');
const targetTileW = document.getElementById('targetTileW');
const targetTileH = document.getElementById('targetTileH');
const targetSpacing = document.getElementById('targetSpacing');
const targetMargin = document.getElementById('targetMargin');
const scaleMode = document.getElementById('scaleMode');
const processTilesetBtn = document.getElementById('processTilesetBtn');
const downloadTilesetBtn = document.getElementById('downloadTilesetBtn');
const tilesetOriginalCanvas = document.getElementById('tilesetOriginalCanvas');
const tilesetResizedCanvas = document.getElementById('tilesetResizedCanvas');
const tilesetEmptyLabel = document.getElementById('tilesetEmptyLabel');
const resizedEmptyLabel = document.getElementById('resizedEmptyLabel');
const originalMeta = document.getElementById('originalMeta');
const resizedMeta = document.getElementById('resizedMeta');

let tilesetImage = null;
let tilesetResizedData = null;
const originalZoom = initZoomPan(document.getElementById('originalWrap'), tilesetOriginalCanvas, document.getElementById('resetOriginalZoom'));
const resizedZoom = initZoomPan(document.getElementById('resizedWrap'), tilesetResizedCanvas, document.getElementById('resetResizedZoom'));

initResizeHandle(document.getElementById('tilesetLeftHandle'), document.getElementById('tilesetLeftSidebar'), document.getElementById('tilesetMain'), 'horizontal', 160);
initResizeHandle(document.getElementById('tilesetPreviewHandle'), document.getElementById('originalWrap').closest('.preview-half'), document.getElementById('tilesetResizedHalf'), 'horizontal', 160);
initResizeHandle(document.getElementById('tilesetSourceHandle'), document.getElementById('tilesetSourceSection'), document.getElementById('tilesetTargetSection'), 'vertical', 60);
initResizeHandle(document.getElementById('tilesetTargetHandle'), document.getElementById('tilesetTargetSection'), document.getElementById('tilesetExportSection'), 'vertical', 60);

tilesetImportBtn.addEventListener('click', () => tilesetFileInput.click());

tilesetFileInput.addEventListener('change', () => {
  const file = tilesetFileInput.files[0];
  if (!file) return;
  const reader = new FileReader();
  reader.onload = e => {
    const img = new Image();
    img.onload = () => {
      tilesetImage = img;
      showOriginalTileset();
      processTilesetBtn.disabled = false;
      downloadTilesetBtn.disabled = true;
      tilesetResizedData = null;
      resizedEmptyLabel.style.display = '';
      tilesetResizedCanvas.style.display = 'none';
      resizedMeta.textContent = '';
    };
    img.src = e.target.result;
  };
  reader.readAsDataURL(file);
  tilesetFileInput.value = '';
});

function showOriginalTileset() {
  tilesetOriginalCanvas.width = tilesetImage.naturalWidth;
  tilesetOriginalCanvas.height = tilesetImage.naturalHeight;
  const ctx = tilesetOriginalCanvas.getContext('2d');
  ctx.drawImage(tilesetImage, 0, 0);
  tilesetOriginalCanvas.style.display = '';
  tilesetEmptyLabel.style.display = 'none';
  originalZoom.center();
  const otw = parseInt(origTileW.value) || 16;
  const oth = parseInt(origTileH.value) || 16;
  const osp = parseInt(origSpacing.value) || 0;
  const om = parseInt(origMargin.value) || 0;
  const cols = Math.floor((tilesetImage.naturalWidth - 2 * om + osp) / (otw + osp));
  const rows = Math.floor((tilesetImage.naturalHeight - 2 * om + osp) / (oth + osp));
  originalMeta.textContent = tilesetImage.naturalWidth + 'x' + tilesetImage.naturalHeight + ' | ~' + cols + 'x' + rows + ' tiles';
}

function processTileset() {
  if (!tilesetImage) return;
  const otw = parseInt(origTileW.value) || 16;
  const oth = parseInt(origTileH.value) || 16;
  const osp = parseInt(origSpacing.value) || 0;
  const om = parseInt(origMargin.value) || 0;
  const ttw = parseInt(targetTileW.value) || 32;
  const tth = parseInt(targetTileH.value) || 32;
  const tsp = parseInt(targetSpacing.value) || 0;
  const tm = parseInt(targetMargin.value) || 0;
  const mode = scaleMode.value;

  const imgW = tilesetImage.naturalWidth;
  const imgH = tilesetImage.naturalHeight;
  const cols = Math.floor((imgW - 2 * om + osp) / (otw + osp));
  const rows = Math.floor((imgH - 2 * om + osp) / (oth + osp));

  if (cols <= 0 || rows <= 0) {
    toast('invalid tile dimensions for image size');
    return;
  }

  const newW = tm + cols * (ttw + tsp) - tsp + tm;
  const newH = tm + rows * (tth + tsp) - tsp + tm;

  tilesetResizedCanvas.width = newW;
  tilesetResizedCanvas.height = newH;
  const ctx = tilesetResizedCanvas.getContext('2d');
  ctx.clearRect(0, 0, newW, newH);

  ctx.imageSmoothingEnabled = mode === 'smooth';
  if (mode === 'nearest') {
    ctx.imageSmoothingEnabled = false;
    ctx.mozImageSmoothingEnabled = false;
    ctx.webkitImageSmoothingEnabled = false;
    ctx.msImageSmoothingEnabled = false;
  }

  for (let row = 0; row < rows; row++) {
    for (let col = 0; col < cols; col++) {
      const sx = om + col * (otw + osp);
      const sy = om + row * (oth + osp);
      const dx = tm + col * (ttw + tsp);
      const dy = tm + row * (tth + tsp);
      ctx.drawImage(tilesetImage, sx, sy, otw, oth, dx, dy, ttw, tth);
    }
  }

  tilesetResizedCanvas.style.display = '';
  resizedEmptyLabel.style.display = 'none';
  resizedZoom.center();
  resizedMeta.textContent = newW + 'x' + newH + ' | ' + cols + 'x' + rows + ' tiles | cell ' + ttw + 'x' + tth;
  tilesetResizedData = true;
  downloadTilesetBtn.disabled = false;
  toast('resized tileset to ' + newW + 'x' + newH);
}

processTilesetBtn.addEventListener('click', processTileset);

const tilesetExportName = document.getElementById('tilesetExportName');

function downloadTileset() {
  if (!tilesetResizedData) return;
  var name = (tilesetExportName.value.trim() || 'tileset') + '.png';
  tilesetResizedCanvas.toBlob(blob => {
    const a = document.createElement('a');
    a.href = URL.createObjectURL(blob);
    a.download = name;
    a.click();
    URL.revokeObjectURL(a.href);
  });
}

downloadTilesetBtn.addEventListener('click', downloadTileset);

/* ─── Bridge ready ─── */
coconut.ready().then(() => {
  toast('atlas-tool ready');
});
