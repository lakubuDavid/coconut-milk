<template>
  <div class="app">
    <nav class="tab-bar">
      <button :class="{ active: page === 'calc' }" @click="page = 'calc'">Calculator</button>
      <button :class="{ active: page === 'settings' }" @click="page = 'settings'">Settings</button>
    </nav>

    <div v-if="page === 'calc'" class="calculator">
      <div class="display">
        <div class="expression">{{ expression || '0' }}</div>
        <div class="result">{{ result !== null ? '= ' + result : '' }}</div>
      </div>
      <div class="buttons">
        <button v-for="btn in buttons" :key="btn.label"
          :class="['btn', btn.cls]" @click="press(btn)">{{ btn.label }}</button>
      </div>
      <button class="clear-history" @click="clearHistory" v-if="history.length">
        Clear History
      </button>
      <div class="history" v-if="history.length">
        <h3>History</h3>
        <div v-for="(entry, i) in history" :key="i" class="entry">
          <span class="expr">{{ entry.expr }}</span>
          <span class="eq">=</span>
          <span class="val">{{ entry.result }}</span>
        </div>
      </div>
    </div>

    <div v-else class="settings">
      <h2>Settings</h2>
      <div class="setting-row">
        <label>Theme</label>
        <select v-model="theme">
          <option value="mint">Mint Green</option>
          <option value="dark">Dark Mode</option>
        </select>
      </div>
      <div class="setting-row">
        <label>Precision</label>
        <input type="number" v-model.number="precision" min="0" max="10" />
      </div>
      <div class="setting-row">
        <label>Sound</label>
        <input type="checkbox" v-model="sound" />
      </div>
      <button class="save-btn" @click="saveSettings">Save Settings</button>
      <div class="status" v-if="status">{{ status }}</div>
    </div>
  </div>
</template>

<script setup>
import { ref, onMounted, onUnmounted } from 'vue'

const page = ref('calc')
const expression = ref('')
const result = ref(null)
const history = ref([])
const theme = ref('mint')
const precision = ref(2)
const sound = ref(false)
const status = ref('')

const buttons = [
  { label: 'C', cls: 'fn', action: 'clear' },
  { label: '±', cls: 'fn', action: 'negate' },
  { label: '%', cls: 'fn', action: 'percent' },
  { label: '÷', cls: 'op', value: '/' },
  { label: '7', cls: 'num', value: '7' },
  { label: '8', cls: 'num', value: '8' },
  { label: '9', cls: 'num', value: '9' },
  { label: '×', cls: 'op', value: '*' },
  { label: '4', cls: 'num', value: '4' },
  { label: '5', cls: 'num', value: '5' },
  { label: '6', cls: 'num', value: '6' },
  { label: '−', cls: 'op', value: '-' },
  { label: '1', cls: 'num', value: '1' },
  { label: '2', cls: 'num', value: '2' },
  { label: '3', cls: 'num', value: '3' },
  { label: '+', cls: 'op', value: '+' },
  { label: '0', cls: 'num wide', value: '0' },
  { label: '.', cls: 'num', value: '.' },
  { label: '=', cls: 'eq', action: 'eval' },
]

function press(btn) {
  if (btn.action === 'clear') { expression.value = ''; result.value = null }
  else if (btn.action === 'negate') {
    if (expression.value) expression.value = expression.value.startsWith('-')
      ? expression.value.slice(1) : '-' + expression.value
  }
  else if (btn.action === 'percent') {
    if (expression.value) expression.value = String(parseFloat(expression.value) / 100)
  }
  else if (btn.action === 'eval') { evaluate() }
  else if (btn.value) { expression.value += btn.value }
}

function evaluate() {
  try {
    const v = Function('"use strict"; return (' +
      expression.value.replace(/×/g,'*').replace(/÷/g,'/').replace(/−/g,'-') + ')')()
    const formatted = Number(v).toFixed(precision.value)
    result.value = parseFloat(formatted)
    history.value.push({ expr: expression.value, result: String(result.value) })
    saveHistory()
  } catch { result.value = 'Error' }
}

async function saveHistory() {
  try { await coconut.call('calc_save', { entries: history.value }) } catch {}
}

async function loadHistory() {
  try { const d = await coconut.call('calc_load', {}); if (d?.entries) history.value = d.entries } catch {}
}

async function clearHistory() { history.value = []; await saveHistory() }

async function loadSettings() {
  try {
    const d = await coconut.call('settings_load', {})
    if (d) {
      theme.value = d.theme || 'mint'
      precision.value = d.precision ?? 2
      sound.value = d.sound ?? false
    }
  } catch {}
}

async function saveSettings() {
  try {
    await coconut.call('settings_save', {
      theme: theme.value,
      precision: precision.value,
      sound: sound.value,
    })
    status.value = 'Saved!'
    setTimeout(() => status.value = '', 2000)
  } catch {
    status.value = 'Failed to save'
  }
}

onMounted(() => {
  loadHistory()
  loadSettings()
})

onUnmounted(() => {
  // Save settings on unmount if needed
})
</script>

<style scoped>
.app { background:#1a2e2a; border-radius:16px; padding:1rem; box-shadow:0 8px 32px rgba(0,0,0,0.4); min-height:600px; }
.tab-bar { display:flex; gap:0.5rem; margin-bottom:1rem; }
.tab-bar button { flex:1; padding:0.6rem; border:none; border-radius:8px; background:#1f3530; color:#90a9a2; cursor:pointer; transition:background .15s; }
.tab-bar button.active { background:#26a69a; color:#e8f4f0; }
.tab-bar button:hover:not(.active) { background:#2a4a42; }
.calculator { }
.display { background:#1a2422; border-radius:10px; padding:1rem; margin-bottom:1rem; text-align:right; min-height:4rem; }
.expression { font-size:1.8rem; word-break:break-all; color:#e8f4f0; }
.result { font-size:1.2rem; color:#7ec8e3; margin-top:0.3rem; }
.buttons { display:grid; grid-template-columns:repeat(4,1fr); gap:0.5rem; }
.btn { padding:1rem; font-size:1.2rem; border:none; border-radius:10px; cursor:pointer; background:#1f3530; color:#c8e6de; transition:background .15s; }
.btn:hover { background:#2a4a42; }
.btn:active { background:#355f55; }
.btn.fn { background:#1a3a45; color:#7ec8e3; }
.btn.fn:hover { background:#1f4a58; }
.btn.op { background:#1a3535; color:#4dd0b0; }
.btn.op:hover { background:#1f4545; }
.btn.eq { background:#26a69a; color:#e8f4f0; }
.btn.eq:hover { background:#2bbbad; }
.btn.wide { grid-column:span 2; }
.clear-history { margin-top:.5rem; padding:.4rem .8rem; font-size:.8rem; border:1px solid #2a4a42; border-radius:6px; background:transparent; color:#90a9a2; cursor:pointer; width:100%; }
.clear-history:hover { background:#1f3530; }
.history { margin-top:1rem; border-top:1px solid #1f3530; padding-top:.5rem; }
.history h3 { font-size:.9rem; color:#90a9a2; margin:0 0 .5rem; }
.entry { display:flex; gap:.5rem; padding:.3rem 0; font-size:.9rem; }
.expr { color:#90a9a2; }
.eq { color:#4a6b63; }
.val { color:#e8f4f0; font-weight:600; }
.settings { color:#c8e6de; }
.settings h2 { color:#e8f4f0; margin-top:0; }
.setting-row { display:flex; align-items:center; justify-content:space-between; padding:0.8rem 0; border-bottom:1px solid #1f3530; }
.setting-row label { color:#90a9a2; }
.setting-row select, .setting-row input[type="number"] { background:#1a2422; border:1px solid #2a4a42; border-radius:6px; padding:0.4rem; color:#e8f4f0; }
.save-btn { margin-top:1rem; padding:0.6rem; width:100%; border:none; border-radius:8px; background:#26a69a; color:#e8f4f0; cursor:pointer; }
.save-btn:hover { background:#2bbbad; }
.status { margin-top:0.5rem; text-align:center; font-size:0.85rem; color:#4dd0b0; }
</style>
