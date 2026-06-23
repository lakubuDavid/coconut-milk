#!/bin/sh
# ── Coconut Milk Installer ──────────────────────────────────────────────────
# Usage:
#   curl -fsSL https://get.coconut-milk.dev | sh
#   curl -fsSL https://raw.githubusercontent.com/lakubuDavid/coconut-milk/main/scripts/install.sh | sh
#
# Options (via env vars):
#   COCONUT_VERSION   Semver tag to install (default: latest release)
#   COCONUT_HOME      Install directory (default: ~/.coconut)
#   COCONUT_DRY_RUN   Set to "1" to preview without downloading
#
# Assumes: sh, curl (or wget), unzip (or busybox unzip), and on Windows: PowerShell
# ─────────────────────────────────────────────────────────────────────────────

set -e

# ── Config ──────────────────────────────────────────────────────────────────

REPO="lakubuDavid/coconut-milk"
VERSION="${COCONUT_VERSION:-latest}"
INSTALL_DIR="${COCONUT_HOME:-$HOME/.coconut}"
BIN_DIR="$INSTALL_DIR/bin"
TMPDIR="${TMPDIR:-/tmp}"

# ── Colors ──────────────────────────────────────────────────────────────────

BOLD='\033[1m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m' # No Color
INFO="${GREEN}✓${NC}"
WARN="${YELLOW}⚠${NC}"
ERR="${RED}✗${NC}"

# ── Helpers ─────────────────────────────────────────────────────────────────

info()   { printf "  $INFO %s\n" "$*"; }
warn()   { printf "  $WARN %s\n" "$*"; }
error()  { printf "  $ERR %s\n" "$*"; exit 1; }
header() { printf "\n${BOLD}%s${NC}\n" "$*"; }

has_cmd() { command -v "$1" >/dev/null 2>&1; }

download() {
  # download <url> <dest>
  if has_cmd curl; then
    curl -fsSL -o "$2" "$1"
  elif has_cmd wget; then
    wget -qO "$2" "$1"
  else
    error "Need curl or wget to download. Install one and try again."
  fi
}

# ── Platform detection ──────────────────────────────────────────────────────

detect_platform() {
  local os arch

  os=$(uname -s | tr '[:upper:]' '[:lower:]')
  arch=$(uname -m)

  case "$os" in
    linux)        PLATFORM="linux"    ;;
    darwin)       PLATFORM="macos"    ;;
    mingw*|msys*|cygwin*) PLATFORM="windows" ;;
    *)            error "Unsupported OS: $os (only macOS, Linux, Windows)" ;;
  esac

  case "$arch" in
    x86_64|amd64) ARCH="x86_64" ;;
    aarch64|arm64) ARCH="arm64"   ;;
    *)            error "Unsupported architecture: $arch (only x86_64, arm64)" ;;
  esac

  # On macOS arm64 runners can target both, but we download native arch
  # On Linux arm64 builds may not exist yet — warn
  if [ "$PLATFORM" = "linux" ] && [ "$ARCH" = "arm64" ]; then
    warn "Linux ARM64 builds are not yet published. Falling back to x86_64 (may need emulation)."
    ARCH="x86_64"
  fi

  # On Windows only x86_64 is built currently
  if [ "$PLATFORM" = "windows" ] && [ "$ARCH" = "arm64" ]; then
    warn "Windows ARM64 builds are not yet published. Using x86_64."
    ARCH="x86_64"
  fi
}

# ── Resolve version ─────────────────────────────────────────────────────────

resolve_version() {
  if [ "$VERSION" = "latest" ]; then
    header "> Resolving latest release..."
    local api_url="https://api.github.com/repos/$REPO/releases/latest"
    local tmp_tag
    if has_cmd curl; then
      tmp_tag=$(curl -fsSL "$api_url" 2>/dev/null | sed -n 's/.*"tag_name": *"\([^"]*\)".*/\1/p')
    elif has_cmd wget; then
      tmp_tag=$(wget -qO- "$api_url" 2>/dev/null | sed -n 's/.*"tag_name": *"\([^"]*\)".*/\1/p')
    fi
    if [ -z "$tmp_tag" ]; then
      # Fallback: our latest release tag is not "latest" because it's prerelease.
      # Use the releases list instead.
      local list_url="https://api.github.com/repos/$REPO/releases?per_page=1"
      if has_cmd curl; then
        tmp_tag=$(curl -fsSL "$list_url" 2>/dev/null | sed -n 's/.*"tag_name": *"\([^"]*\)".*/\1/p' | head -1)
      elif has_cmd wget; then
        tmp_tag=$(wget -qO- "$list_url" 2>/dev/null | sed -n 's/.*"tag_name": *"\([^"]*\)".*/\1/p' | head -1)
      fi
    fi
    if [ -z "$tmp_tag" ]; then
      error "Could not determine latest release. Set COCONUT_VERSION manually (e.g. v0.1.0.alpha-1)"
    fi
    VERSION="$tmp_tag"
    info "Latest release: $VERSION"
  fi
}

# ── Install ─────────────────────────────────────────────────────────────────

do_install() {
  detect_platform
  resolve_version

  header "Coconut Milk $VERSION for $PLATFORM ($ARCH)"
  echo "  Installing to: $BIN_DIR"

  mkdir -p "$BIN_DIR"

  local bin_url
  local archive_name
  local inner_name

  if [ "$PLATFORM" = "windows" ]; then
    archive_name="coconut-windows-$ARCH.zip"
    inner_name="coconut-windows-$ARCH.exe"
    final_name="coconut.exe"
  else
    archive_name="coconut-$PLATFORM-$ARCH.zip"
    inner_name="coconut-$PLATFORM-$ARCH"
    final_name="coconut"
  fi

  bin_url="https://github.com/$REPO/releases/download/$VERSION/$archive_name"

  # ── Download binary archive ─────────────────────────────────────────────
  printf "  Downloading ${BOLD}%s${NC} ... " "$archive_name"
  local tmp_zip="$TMPDIR/_coconut_install_$$.zip"
  download "$bin_url" "$tmp_zip"
  echo "done"

  # ── Extract ────────────────────────────────────────────────────────────
  printf "  Extracting ... "
  local tmp_extract="$TMPDIR/_coconut_extract_$$"
  mkdir -p "$tmp_extract"

  if has_cmd unzip; then
    unzip -qo "$tmp_zip" -d "$tmp_extract"
  else
    # busybox unzip
    busybox unzip -qo "$tmp_zip" -d "$tmp_extract" 2>/dev/null || \
      error "Need unzip. Install it and try again."
  fi
  rm -f "$tmp_zip"

  # Find the extracted binary — try multiple naming patterns
  local extracted=""
  for name in "$inner_name" "${inner_name}-unstripped" "coconut" "coconut.exe"; do
    extracted=$(find "$tmp_extract" -type f -name "$name" 2>/dev/null | head -1)
    if [ -n "$extracted" ] && [ -f "$extracted" ]; then break; fi
  done

  if [ -z "$extracted" ] || [ ! -f "$extracted" ]; then
    # Last resort: grab whatever file is there
    extracted=$(find "$tmp_extract" -type f 2>/dev/null | head -1)
  fi

  if [ -z "$extracted" ] || [ ! -f "$extracted" ]; then
    error "Binary not found in archive. Contents: $(ls -la "$tmp_extract" 2>/dev/null)"
  fi
  echo "done"

  # ── Install binary ──────────────────────────────────────────────────────
  printf "  Installing ... "
  cp "$extracted" "$BIN_DIR/$final_name"
  chmod +x "$BIN_DIR/$final_name"
  rm -rf "$tmp_extract"
  echo "done"

  # ── Install create-coconut-app script ───────────────────────────────────
  printf "  Installing create-coconut-app ... "
  local script_url="https://raw.githubusercontent.com/$REPO/main/scripts/create-coconut-app"
  download "$script_url" "$BIN_DIR/create-coconut-app"
  chmod +x "$BIN_DIR/create-coconut-app"
  echo "done"

  # ── Summary ─────────────────────────────────────────────────────────────
  printf "\n"
  info "Installed coconut $VERSION"
  info "Installed create-coconut-app"

  printf "\n"
  header "> Add to PATH"
  echo "  Add the following to your shell profile (~/.bashrc, ~/.zshrc, etc.):"
  echo ""
  echo "      export PATH=\"\$PATH:$BIN_DIR\""
  echo ""
  echo "  Or run immediately:"
  echo ""
  echo "      export PATH=\"\$PATH:$BIN_DIR\""
  echo "      coconut --version"
  echo "      create-coconut-app --help"
  echo ""

  # Try to detect if already on PATH
  case ":$PATH:" in
    *":$BIN_DIR:"*) info "Already on PATH" ;;
    *)
      # Offer to auto-add for common shells
      for rc in "$HOME/.bashrc" "$HOME/.zshrc" "$HOME/.profile"; do
        if [ -f "$rc" ] && [ ! -f "$rc.coconut-backup" ]; then
          warn "To auto-add, run:  echo 'export PATH=\"\$PATH:$BIN_DIR\"' >> $rc"
          break
        fi
      done
      ;;
  esac

  printf "\n"
  info "Done — happy building!"
}

# ── Dry run ─────────────────────────────────────────────────────────────────

if [ "${COCONUT_DRY_RUN:-0}" = "1" ]; then
  detect_platform
  resolve_version 2>/dev/null || true
  echo "Platform:   $PLATFORM"
  echo "Arch:       $ARCH"
  echo "Version:    $VERSION"
  echo "Install to: $BIN_DIR"
  echo ""
  echo "Would download:"
  echo "  https://github.com/$REPO/releases/download/$VERSION/coconut-$PLATFORM-$ARCH.zip"
  echo "  https://raw.githubusercontent.com/$REPO/main/scripts/create-coconut-app"
  exit 0
fi

# ── Run ─────────────────────────────────────────────────────────────────────

do_install
exit 0
