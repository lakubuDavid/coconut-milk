set shell := ["bash", "-uc"]

DEFAULT_TARGET := "coconut"
TEST_TARGET := "coconut-milk-tests"

# ── Install paths ───────────────────────────────────────────
INSTALL_DIR := "$HOME/tools"

default:
	@just --list

build:
	xmake build {{DEFAULT_TARGET}}

run:
	xmake run {{DEFAULT_TARGET}}

run-gen:
	xmake run coconut generate

# ── Examples (run core binary from example directory) ───────

run-editor: build build-editor-bundle
	xmake run --workdir=$(pwd)/examples/code-editor {{DEFAULT_TARGET}}

run-ocr: build
	xmake run --workdir=$(pwd)/examples/ocr-app {{DEFAULT_TARGET}}

run-lua-html: build
	xmake run --workdir=$(pwd)/examples/lua-html-app {{DEFAULT_TARGET}}

run-vue: build
	xmake run --workdir=$(pwd)/examples/calculator-vue {{DEFAULT_TARGET}}

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
