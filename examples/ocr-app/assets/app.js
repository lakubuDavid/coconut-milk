function ocrApp() {
  return {
    activeTab: 'scan',
    dragging: false,
    imageUrl: null,
    imagePath: null,
    text: '',
    error: null,
    scanning: false,
    history: [],
    settings: {
      language: 'eng',
      psm: '3',
      autoCopy: false,
      darkMode: true,
    },

    async init() {
      await coconut.ready()
      this.loadHistory()
      this.loadSettings()
    },

    handleDrop(e) {
      this.dragging = false
      const file = e.dataTransfer.files[0]
      if (file) this.loadFile(file)
    },

    handleFile(e) {
      const file = e.target.files[0]
      if (file) this.loadFile(file)
    },

    loadFile(file) {
      this.clearImage()
      this.imageUrl = URL.createObjectURL(file)
      this.saveToTemp(file)
    },

    async saveToTemp(file) {
      const reader = new FileReader()
      reader.onload = async () => {
        const base64 = reader.result.split(',')[1]
        try {
          const result = await coconut.call('ocr_save_temp', { name: file.name, data: base64 })
          if (result.ok) {
            this.imagePath = result.path
          } else {
            this.error = result.error || 'Failed to save image'
          }
        } catch (e) {
          this.error = e.message || 'Failed to save image'
        }
      }
      reader.readAsDataURL(file)
    },

    async scan() {
      if (!this.imagePath) return
      this.scanning = true
      this.error = null
      this.text = ''

      try {
        const result = await coconut.call('ocr_scan', { image_path: this.imagePath })
        if (result.ok) {
          this.text = result.text
          if (this.settings.autoCopy && this.text) {
            navigator.clipboard.writeText(this.text)
          }
        } else {
          this.error = result.error || 'OCR failed'
        }
      } catch (e) {
        this.error = e.message || 'Scan failed'
      }

      this.scanning = false
    },

    clearImage() {
      this.imageUrl = null
      this.imagePath = null
      this.text = ''
      this.error = null
    },

    copyText() {
      navigator.clipboard.writeText(this.text)
        .then(() => this.showToast('Copied to clipboard'))
        .catch(() => this.showToast('Failed to copy'))
    },

    async saveText() {
      if (!this.text) return
      try {
        const result = await coconut.dialog.save('Save OCR Text', 'ocr-output.txt')
        if (result.confirmed && result.path) {
          const saveResult = await coconut.call('ocr_save_text', { text: this.text, filename: result.path })
          if (saveResult.ok) {
            this.showToast('Saved to: ' + result.path)
          } else {
            this.showToast('Failed to save: ' + (saveResult.error || 'unknown'))
          }
        }
      } catch (e) {
        this.showToast('Failed: ' + e.message)
      }
    },

    saveToHistory() {
      if (!this.text) return
      const item = { text: this.text, date: new Date().toLocaleString() }
      this.history.unshift(item)
      this.persistHistory()
      this.showToast('Saved to history')
    },

    loadFromHistory(item) {
      this.text = item.text
      this.activeTab = 'scan'
    },

    deleteFromHistory(index) {
      this.history.splice(index, 1)
      this.persistHistory()
    },

    clearHistory() {
      if (confirm('Clear all history?')) {
        this.history = []
        this.persistHistory()
      }
    },

    persistHistory() {
      try {
        localStorage.setItem('ocr_history', JSON.stringify(this.history.slice(0, 50)))
      } catch (e) {}
    },

    loadHistory() {
      try {
        const data = localStorage.getItem('ocr_history')
        if (data) this.history = JSON.parse(data)
      } catch (e) {}
    },

    saveSettings() {
      try {
        localStorage.setItem('ocr_settings', JSON.stringify(this.settings))
      } catch (e) {}
    },

    loadSettings() {
      try {
        const data = localStorage.getItem('ocr_settings')
        if (data) this.settings = { ...this.settings, ...JSON.parse(data) }
      } catch (e) {}
    },

    showToast(msg) {
      alert(msg)
    },
  }
}