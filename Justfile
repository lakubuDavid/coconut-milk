set shell := ["bash", "-uc"]

DEFAULT_TARGET := "coconut"
TEST_TARGET := "coconut-milk-tests"

# ── Install paths ───────────────────────────────────────────
INSTALL_DIR := "$HOME/tools"

# Resolve binary path once (avoids --workdir fragility)
# Binary lives at a predictable path after xmake build
BIN := justfile_directory() / "build/macosx/x86_64/debug/coconut"

default:
	@just --list

build:
	xmake build {{DEFAULT_TARGET}}

run:
	xmake run {{DEFAULT_TARGET}}

run-gen:
	xmake run coconut generate

# ── Examples (cd into example dir, run binary directly) ─────

run-editor: build build-editor-bundle
	cd examples/code-editor && {{BIN}}

run-ocr: build
	cd examples/ocr-app && {{BIN}}

run-lua-html: build
	cd examples/lua-html-app && {{BIN}}

run-vue: build
	cd examples/calculator-vue && {{BIN}}

run-atlas: build
	cd examples/atlas-tool && {{BIN}}

# Build the CodeMirror 6 bundle for the code-editor example
build-editor-bundle:
	cd examples/code-editor && bun run build-bundle

# ── Tests ───────────────────────────────────────────────────

test:
	xmake build {{TEST_TARGET}}
	xmake run {{TEST_TARGET}}

# ── Housekeeping ────────────────────────────────────────────

clean:
	xmake clean

rebuild:
	just clean
	just build

debug:
	xmake f -m debug
	just build
	just run

format:
	clang-format -i src/**/*.h src/**/*.cpp tests/**/*.cpp

install:
	mkdir -p {{INSTALL_DIR}}
	ln -sf "$(pwd)/build/macosx/x86_64/debug/coconut" "{{INSTALL_DIR}}/coconut"
	ln -sf "$(pwd)/scripts/create-coconut-app" "{{INSTALL_DIR}}/create-coconut-app"
	@echo "installed to {{INSTALL_DIR}}:"
	@ls -la "{{INSTALL_DIR}}/coconut" "{{INSTALL_DIR}}/create-coconut-app"
