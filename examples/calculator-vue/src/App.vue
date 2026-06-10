<template>
  <div class="calculator">
    <div class="display">
      <div class="expression">{{ expression || '0' }}</div>
      <div class="result">{{ result !== null ? '= ' + result : '' }}</div>
    </div>

    <div class="buttons">
      <button v-for="btn in buttons" :key="btn.label"
        :class="['btn', btn.cls]"
        @click="press(btn)">
        {{ btn.label }}
      </button>
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
</template>

<script>
import { calc_save as calcSave, calc_load as calcLoad } from './calc.g.js'

export default {
  data() {
    return {
      expression: '',
      result: null,
      history: [],
      buttons: [
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
      ],
    }
  },

  async mounted() {
    await this.loadHistory()
  },

  methods: {
    press(btn) {
      if (btn.action === 'clear') {
        this.expression = ''
        this.result = null
      } else if (btn.action === 'negate') {
        if (this.expression) {
          this.expression = this.expression.startsWith('-')
            ? this.expression.slice(1)
            : '-' + this.expression
        }
      } else if (btn.action === 'percent') {
        if (this.expression) {
          this.expression = String(parseFloat(this.expression) / 100)
        }
      } else if (btn.action === 'eval') {
        this.evaluate()
      } else if (btn.value) {
        this.expression += btn.value
      }
    },

    evaluate() {
      try {
        const sanitized = this.expression
          .replace(/×/g, '*')
          .replace(/÷/g, '/')
          .replace(/−/g, '-')
        const val = Function('"use strict"; return (' + sanitized + ')')()
        this.result = val
        const entry = { expr: this.expression, result: String(val) }
        this.history.push(entry)
        this.saveHistory()
      } catch {
        this.result = 'Error'
      }
    },

    async saveHistory() {
      try {
        await calcSave({ entries: this.history })
      } catch (e) {
        console.warn('save failed:', e)
      }
    },

    async loadHistory() {
      try {
        const data = await calcLoad({})
        if (data && Array.isArray(data.entries)) {
          this.history = data.entries
        }
      } catch {
        // no history yet
      }
    },

    async clearHistory() {
      this.history = []
      await this.saveHistory()
    },
  },
}
</script>

<style scoped>
.calculator {
  background: #22222e;
  border-radius: 16px;
  padding: 1rem;
  box-shadow: 0 8px 32px rgba(0,0,0,0.4);
}
.display {
  background: #2a2a3a;
  border-radius: 10px;
  padding: 1rem;
  margin-bottom: 1rem;
  text-align: right;
  min-height: 4rem;
}
.expression {
  font-size: 1.8rem;
  word-break: break-all;
}
.result {
  font-size: 1.2rem;
  color: #a78bfa;
  margin-top: 0.3rem;
}
.buttons {
  display: grid;
  grid-template-columns: repeat(4, 1fr);
  gap: 0.5rem;
}
.btn {
  padding: 1rem;
  font-size: 1.2rem;
  border: none;
  border-radius: 10px;
  cursor: pointer;
  background: #2a2a3a;
  color: #e4e4e7;
  transition: background 0.15s;
}
.btn:hover { background: #3a3a4a; }
.btn:active { background: #4a4a5a; }
.btn.fn { background: #3a2a5a; color: #c4a0ff; }
.btn.fn:hover { background: #4a3a6a; }
.btn.op { background: #2a3a4a; color: #60a5fa; }
.btn.op:hover { background: #3a4a5a; }
.btn.eq { background: #7c3aed; color: #fff; }
.btn.eq:hover { background: #8b4af5; }
.btn.wide { grid-column: span 2; }
.clear-history {
  margin-top: 0.5rem;
  padding: 0.4rem 0.8rem;
  font-size: 0.8rem;
  border: 1px solid #3a3a4a;
  border-radius: 6px;
  background: transparent;
  color: #a1a1aa;
  cursor: pointer;
  width: 100%;
}
.clear-history:hover { background: #2a2a3a; }
.history {
  margin-top: 1rem;
  border-top: 1px solid #2a2a3a;
  padding-top: 0.5rem;
}
.history h3 {
  font-size: 0.9rem;
  color: #a1a1aa;
  margin: 0 0 0.5rem;
}
.entry {
  display: flex;
  gap: 0.5rem;
  padding: 0.3rem 0;
  font-size: 0.9rem;
}
.expr { color: #a1a1aa; }
.eq { color: #52525b; }
.val { color: #e4e4e7; font-weight: 600; }
</style>
