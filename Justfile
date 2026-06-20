set shell := ["bash", "-uc"]

DEFAULT_TARGET  := "coconut"
TEST_TARGET     := "coconut-milk-tests"
MODE            := "debug"
PROJECT_ROOT    := justfile_directory()

# ── Install paths ───────────────────────────────────────────
INSTALL_DIR := "$HOME/tools"

default:
	@just --list

# ── Internal helpers ────────────────────────────────────────

# Print the full path to the target binary (platform-agnostic).
# Relies on xmake show which already knows the current plat/arch.
_targetfile TARGET:
	@xmake show --target={{TARGET}} 2>/dev/null \
	  | awk -F': ' '{gsub(/\033\[[0-9;]*m/,"",$0); if ($1 ~ /targetfile$/) { printf "%s/%s", "{{PROJECT_ROOT}}", $2; print "" }}'

# ── Build ───────────────────────────────────────────────────

build:
	xmake f -m {{MODE}} -c -y
	xmake build {{DEFAULT_TARGET}}

build-release:
	@just --set MODE release build

build-debug:
	@just --set MODE debug build

# ── Run (uses xmake run — no path needed) ──────────────────

run: build
	xmake run {{DEFAULT_TARGET}}

run-release:
	@just --set MODE release run

run-debug:
	@just --set MODE debug run

# ── Examples (cd into example dir, resolve binary via xmake) ─

run-editor: build build-editor-bundle
	cd examples/code-editor && exec "$$(cd {{PROJECT_ROOT}} && just --justfile {{PROJECT_ROOT}}/Justfile _targetfile {{DEFAULT_TARGET}})"

run-editor-release:
	@just --set MODE release run-editor

run-editor-debug:
	@just --set MODE debug run-editor

run-ocr: build
	cd examples/ocr-app && exec "$$(cd {{PROJECT_ROOT}} && just --justfile {{PROJECT_ROOT}}/Justfile _targetfile {{DEFAULT_TARGET}})"

run-ocr-release:
	@just --set MODE release run-ocr

run-lua-html: build
	cd examples/lua-html-app && exec "$$(cd {{PROJECT_ROOT}} && just --justfile {{PROJECT_ROOT}}/Justfile _targetfile {{DEFAULT_TARGET}})"

run-lua-html-release:
	@just --set MODE release run-lua-html

run-vue: build
	cd examples/calculator-vue && exec "$$(cd {{PROJECT_ROOT}} && just --justfile {{PROJECT_ROOT}}/Justfile _targetfile {{DEFAULT_TARGET}})"

run-vue-release:
	@just --set MODE release run-vue

run-atlas: build
	cd examples/atlas-tool && exec "$$(cd {{PROJECT_ROOT}} && just --justfile {{PROJECT_ROOT}}/Justfile _targetfile {{DEFAULT_TARGET}})"

run-atlas-release:
	@just --set MODE release run-atlas

run-playground: build
	cd examples/playground && \
	  BIN="$$(cd {{PROJECT_ROOT}} && just --justfile {{PROJECT_ROOT}}/Justfile _targetfile {{DEFAULT_TARGET}})" && \
	  "$$BIN" generate && \
	  exec "$$BIN"

run-playground-release:
	@just --set MODE release run-playground

run-gen:
	xmake run coconut generate

# Build the CodeMirror 6 bundle for the code-editor example
build-editor-bundle:
	cd examples/code-editor && bun run build-bundle

# ── Tests ───────────────────────────────────────────────────

test:
	xmake f -m {{MODE}} -c -y
	xmake build {{TEST_TARGET}}
	xmake run {{TEST_TARGET}}

test-release:
	@just --set MODE release test

test-debug:
	@just --set MODE debug test

# ── Housekeeping ────────────────────────────────────────────

clean:
	xmake clean

rebuild:
	just clean
	just build

format:
	clang-format -i src/**/*.h src/**/*.cpp tests/**/*.cpp

install: build
	mkdir -p {{INSTALL_DIR}}
	ln -sf "$$(cd {{PROJECT_ROOT}} && just --justfile {{PROJECT_ROOT}}/Justfile _targetfile {{DEFAULT_TARGET}})" "{{INSTALL_DIR}}/coconut"
	ln -sf "{{PROJECT_ROOT}}/scripts/create-coconut-app" "{{INSTALL_DIR}}/create-coconut-app"
	@echo "installed to {{INSTALL_DIR}}:"
	@ls -la "{{INSTALL_DIR}}/coconut" "{{INSTALL_DIR}}/create-coconut-app"
