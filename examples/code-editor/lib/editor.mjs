// CodeMirror 6 bundle entry point.
// Build:  cd examples/code-editor && bun run build-bundle
// Output: assets/editor-bundle.js  (loaded as <script type="module">)

import { EditorView, basicSetup } from "codemirror";

import { css }     from "@codemirror/lang-css";
import { html }    from "@codemirror/lang-html";
import { javascript } from "@codemirror/lang-javascript";
import { json }    from "@codemirror/lang-json";
import { markdown } from "@codemirror/lang-markdown";
import { python }  from "@codemirror/lang-python";
import { sql }     from "@codemirror/lang-sql";
import { xml }     from "@codemirror/lang-xml";

import { StreamLanguage } from "@codemirror/language";
import { lua }            from "@codemirror/legacy-modes/mode/lua";
import { c, cpp, java, kotlin, dart, objectiveC, scala, shader }
                          from "@codemirror/legacy-modes/mode/clike";

import { keymap } from "@codemirror/view";

import { dracula } from "thememirror";

// ── Language registry ──────────────────────────────────────────

const languageMap = {
  // Built-in CM6 packages
  css:        () => css(),
  html:       () => html(),
  javascript: () => javascript(),
  jsx:        () => javascript({ jsx: true }),
  typescript: () => javascript({ typescript: true }),
  tsx:        () => javascript({ tsx: true }),
  json:       () => json(),
  markdown:   () => markdown(),
  python:     () => python(),
  sql:        () => sql(),
  xml:        () => xml(),

  // Legacy-stream modes (ported from CM5)
  lua:       () => StreamLanguage.define(lua),
  c:         () => StreamLanguage.define(c),
  cpp:       () => StreamLanguage.define(cpp),
  java:      () => StreamLanguage.define(java),
  kotlin:    () => StreamLanguage.define(kotlin),
  dart:      () => StreamLanguage.define(dart),
  objectivec: () => StreamLanguage.define(objectiveC),
  scala:     () => StreamLanguage.define(scala),
  glsl:      () => StreamLanguage.define(shader),
};

// ── File extension → language lookup ──────────────────────────

const extMap = {
  ".js":     "javascript",    ".mjs": "javascript",
  ".cjs":    "javascript",    ".jsx": "jsx",
  ".ts":     "typescript",    ".tsx": "tsx",
  ".css":    "css",           ".scss": "css",
  ".less":   "css",
  ".html":   "html",          ".htm":  "html",
  ".json":   "json",
  ".md":     "markdown",      ".markdown": "markdown",
  ".py":     "python",        ".pyw": "python",
  ".sql":    "sql",
  ".xml":    "xml",           ".svg":  "xml",
  ".plist":  "xml",
  ".lua":    "lua",
  ".c":      "c",             ".h":    "c",
  ".cpp":    "cpp",           ".hpp":  "cpp",
  ".cc":     "cpp",           ".cxx":  "cpp",
  ".java":   "java",
  ".kt":     "kotlin",        ".kts":  "kotlin",
  ".dart":   "dart",
  ".m":      "objectivec",    ".mm":   "objectivec",
  ".scala":  "scala",
  ".glsl":   "glsl",          ".vert": "glsl",
  ".frag":   "glsl",
};

function detectLanguage(filename) {
  if (!filename) return null;
  const dot = filename.lastIndexOf(".");
  if (dot === -1) return null;
  const ext = filename.slice(dot).toLowerCase();
  return extMap[ext] || null;
}

// ── Editor theme ───────────────────────────────────────────────

const coconutTheme = EditorView.theme({
  "&": {
    backgroundColor: "#282a36",
    height: "100%",
  },
  ".cm-scroller": {
    fontFamily: "'Fira Code', 'JetBrains Mono', 'Menlo', monospace",
    fontSize: "14px",
  },
  ".cm-gutters": {
    backgroundColor: "#282a36",
    color: "#6272a4",
    border: "none",
  },
  ".cm-activeLineGutter": {
    backgroundColor: "#3a3d4f",
  },
  ".cm-foldPlaceholder": {
    backgroundColor: "#3a3d4f",
    color: "#6272a4",
    border: "none",
    borderRadius: "3px",
    padding: "0 4px",
  },
});

// ── Public API ─────────────────────────────────────────────────

/**
 * Create a CodeMirror 6 editor inside parent.
 *
 * @param {HTMLElement}  parent        DOM node to mount into
 * @param {object}       opts
 * @param {string}       opts.content  Initial document text
 * @param {string|null}  opts.language Language key from languageMap
 * @param {function|null} opts.onSave  Called when Ctrl/Cmd+S is pressed
 * @returns {EditorView}
 */
window.createCoconutEditor = (parent, opts = {}) => {
  const extensions = [
    basicSetup,
    dracula,
    coconutTheme,
  ];

  // Language
  const langId = opts.language || detectLanguage(opts.filename);
  if (langId && languageMap[langId]) {
    extensions.push(languageMap[langId]());
  }

  // Save keybinding
  if (opts.onSave) {
    extensions.push(keymap.of([
      { key: "Mod-s", run: () => { opts.onSave(); return true; } },
    ]));
  }

  const view = new EditorView({
    doc: opts.content || "",
    extensions,
    parent,
  });

  return view;
};
