set shell := ["bash", "-uc"]

DEFAULT_TARGET := "coconut"
TEST_TARGET := "coconut-milk-tests"

# ── Install paths ───────────────────────────────────────────
# Default: user-local tools directory.
# Industry standard: INSTALL_DIR := "/usr/local/bin"
INSTALL_DIR := "$HOME/tools"

default:
	@just --list

build:
	xmake build {{DEFAULT_TARGET}}

build-vue:
	xmake build calculator-vue
	cd examples/calculator-vue && bun run build

run:
	xmake run {{DEFAULT_TARGET}}

run-vue:
	xmake build calculator-vue
	COCONUT_DEV=1 xmake run calculator-vue

run-vue-prod:
	xmake build calculator-vue
	cd examples/calculator-vue && bun run build
	xmake run calculator-vue

run-ocr:
	xmake build ocr-app
	xmake run ocr-app

run-lua-html:
	xmake build lua-html-app
	xmake run lua-html-app

test:
	xmake build {{TEST_TARGET}}
	xmake run {{TEST_TARGET}}

run-gen:
	xmake run coconut generate

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

# Symlink coconut + create-coconut-app into INSTALL_DIR.
install:
	mkdir -p {{INSTALL_DIR}}
	ln -sf "$(pwd)/build/macosx/x86_64/debug/coconut" "{{INSTALL_DIR}}/coconut"
	ln -sf "$(pwd)/scripts/create-coconut-app" "{{INSTALL_DIR}}/create-coconut-app"
	@echo "installed to {{INSTALL_DIR}}:"
	@ls -la "{{INSTALL_DIR}}/coconut" "{{INSTALL_DIR}}/create-coconut-app"
