#!/bin/sh
# ── Coconut Milk Installer ──────────────────────────────────────────────────
# Usage:
#   curl -fsSL https://lakubudavid.me/coconut-milk/install.sh | sh
#   curl -fsSL https://raw.githubusercontent.com/lakubuDavid/coconut-milk/main/scripts/install.sh | sh
#
# Installs the latest Coconut Milk release binaries (`coconut`, `coconut-cli`)
# into a configurable bin directory (default ~/.local/bin), and the full
# create-coconut-app tools bundle (script, vendored argparse.lua, schemas, agent
# skill) into ~/.coconut. The script is symlinked into the bin dir so it is on
# PATH; everything it needs resolves from ~/.coconut at runtime.
#
# Options:
#   -y, --yes          Confirm without prompting (non-interactive)
#   --dry-run          Show the planned install and exit (no download)
#   --version VER      Install a specific tag, e.g. v0.1.0.alpha-1
#   --bin-dir DIR      Install directory for binaries (default ~/.local/bin)
#   --home DIR         Directory for other assets (default ~/.coconut)
#   -h, --help         Show this help
#
# Environment overrides:
#   COCONUT_YES=1        same as --yes
#   COCONUT_VERSION=VER  same as --version
#   COCONUT_BIN_DIR=DIR  same as --bin-dir
#   COCONUT_HOME=DIR     same as --home
#   COCONUT_DRY_RUN=1    same as --dry-run
#   NO_COLOR=1           disable color output
# ─────────────────────────────────────────────────────────────────────────────

set -eu

# ── Config ──────────────────────────────────────────────────────────────────

REPO="lakubuDavid/coconut-milk"
REQUESTED_VERSION="${COCONUT_VERSION:-}"
BIN_DIR="${COCONUT_BIN_DIR:-$HOME/.local/bin}"
COCONUT_HOME="${COCONUT_HOME:-$HOME/.coconut}"
YES="${COCONUT_YES:-0}"
DRY_RUN="${COCONUT_DRY_RUN:-0}"
TMPDIR="${TMPDIR:-/tmp}"
POSITIONAL_BIN_DIR=

# ── Colors ──────────────────────────────────────────────────────────────────

if [ -t 1 ] && [ -z "${NO_COLOR:-}" ]; then
  CYAN='\033[38;2;116;141;166m'; GREEN='\033[38;2;156;180;204m'
  LAVENDER='\033[38;2;211;206;223m'; YELLOW='\033[38;2;242;215;217m'
  RED='\033[0;31m'; BOLD='\033[1m'; RESET='\033[0m'
else
  CYAN=; GREEN=; LAVENDER=; YELLOW=; RED=; BOLD=; RESET=
fi

info()  { printf '  %b%s%b\n' "${GREEN}✓${RESET}" "$*" "${RESET}"; }
warn()  { printf '  %b%s%b\n' "${YELLOW}⚠${RESET}" "$*" "${RESET}"; }
err()   { printf '  %b%s%b\n' "${RED}✗${RESET}" "$*" "${RESET}"; exit 1; }
header(){ printf '\n%b%s%b\n' "${BOLD}" "$*" "${RESET}"; }

has_cmd() { command -v "$1" >/dev/null 2>&1; }

# ── Banner (pregenerated: figlet -f rounded "coconut-milk") ─────────────────
# Each line is a single-quoted literal so backslashes/spaces are preserved
# verbatim; %s prints them literally (never reinterpreted by printf).

print_banner() {
  printf '%b%s%b\n' "$CYAN"    '                                                     _ _  _'     "$RESET"
  printf '%b%s%b\n' "$GREEN"   '                                     _              (_) || |' "$RESET"
  printf '%b%s%b\n' "$LAVENDER" '  ____ ___   ____ ___  ____  _   _ _| |_ _____ ____  _| || |  _' "$RESET"
  printf '%b%s%b\n' "$CYAN"    ' / ___) _ \ / ___) _ \|  _ \| | | (_   _|_____)    \| | || |_/ )' "$RESET"
  printf '%b%s%b\n' "$GREEN"   '( (__| |_| ( (__| |_| | | | | |_| | | |_      | | | | | ||  _ (' "$RESET"
  printf '%b%s%b\n' "$LAVENDER" ' \____)___/ \____)___/|_| |_|____/   \__)     |_|_|_|_|\_)_| \_)' "$RESET"
}

# ── Argument parsing ─────────────────────────────────────────────────────────

while [ "$#" -gt 0 ]; do
  case "$1" in
    -y|--yes) YES=1 ;;
    --dry-run) DRY_RUN=1 ;;
    --version)
      [ "$#" -ge 2 ] || err "--version requires a value"
      REQUESTED_VERSION=$2; shift ;;
    --version=*) REQUESTED_VERSION=${1#--version=} ;;
    --bin-dir)
      [ "$#" -ge 2 ] || err "--bin-dir requires a value"
      BIN_DIR=$2; shift ;;
    --bin-dir=*) BIN_DIR=${1#--bin-dir=} ;;
    --home)
      [ "$#" -ge 2 ] || err "--home requires a value"
      COCONUT_HOME=$2; shift ;;
    --home=*) COCONUT_HOME=${1#--home=} ;;
    -h|--help)
      sed -n '2,34p' "$0" | sed 's/^# \{0,1\}//'
      exit 0 ;;
    --) shift; break ;;
    -*) err "unknown option: $1" ;;
    *)
      [ -z "$POSITIONAL_BIN_DIR" ] || err "only one positional bin directory is allowed"
      POSITIONAL_BIN_DIR=$1 ;;
  esac
  shift
done
[ -z "$POSITIONAL_BIN_DIR" ] || BIN_DIR=$POSITIONAL_BIN_DIR

# ── Platform detection (offline) ────────────────────────────────────────────

detect_platform() {
  local os arch
  os=$(uname -s | tr '[:upper:]' '[:lower:]')
  arch=$(uname -m)
  case "$os" in
    linux) PLATFORM="linux" ;;
    darwin) PLATFORM="macos" ;;
    mingw*|msys*|cygwin*) PLATFORM="windows" ;;
    *) err "Unsupported OS: $os (only macOS, Linux, Windows)" ;;
  esac
  case "$arch" in
    x86_64|amd64) ARCH="x86_64" ;;
    aarch64|arm64) ARCH="arm64" ;;
    *) err "Unsupported architecture: $arch (only x86_64, arm64)" ;;
  esac
  if [ "$PLATFORM" = "linux" ] && [ "$ARCH" = "arm64" ]; then
    warn "Linux ARM64 builds not published yet; falling back to x86_64."
    ARCH="x86_64"
  fi
  if [ "$PLATFORM" = "windows" ] && [ "$ARCH" = "arm64" ]; then
    warn "Windows ARM64 builds not published yet; using x86_64."
    ARCH="x86_64"
  fi
}

# ── Resolve version (network) ───────────────────────────────────────────────

resolve_version() {
  VERSION="${REQUESTED_VERSION:-}"
  if [ -z "$VERSION" ]; then
    header "> Resolving latest release..."
    local api_url="https://api.github.com/repos/$REPO/releases/latest"
    local tmp
    tmp=$(curl -fsSL "$api_url" 2>/dev/null | sed -n 's/.*"tag_name": *"\([^"]*\)".*/\1/p')
    if [ -z "$tmp" ]; then
      local list_url="https://api.github.com/repos/$REPO/releases?per_page=1"
      tmp=$(curl -fsSL "$list_url" 2>/dev/null | sed -n 's/.*"tag_name": *"\([^"]*\)".*/\1/p' | head -1)
    fi
    [ -n "$tmp" ] || err "Could not determine latest release (set --version, e.g. v0.1.0.alpha-1)"
    VERSION="$tmp"
    info "Latest release: $VERSION"
  fi
}

# ── Download helper ──────────────────────────────────────────────────────────

download() {
  if has_cmd curl; then curl -fsSL -o "$2" "$1"
  elif has_cmd wget; then wget -qO "$2" "$1"
  else err "Need curl or wget to download."; fi
}

# ── Install ──────────────────────────────────────────────────────────────────

do_install() {
  detect_platform
  resolve_version

  header "Coconut Milk $VERSION for $PLATFORM ($ARCH)"
  echo "  Binaries : $BIN_DIR"
  echo "  Assets   : $COCONUT_HOME"
  mkdir -p "$BIN_DIR" "$COCONUT_HOME"

  local archive inner final
  if [ "$PLATFORM" = "windows" ]; then
    archive="coconut-windows-$ARCH.zip"; inner="coconut.exe"; final="coconut.exe"
  else
    archive="coconut-$PLATFORM-$ARCH.zip"; inner="coconut"; final="coconut"
  fi
  local bin_url="https://github.com/$REPO/releases/download/$VERSION/$archive"

  printf "  Downloading %s ... " "$archive"
  local tmp_zip="$TMPDIR/_coconut_install_$$.zip"
  download "$bin_url" "$tmp_zip" || err "download failed: $bin_url"
  echo "done"

  printf "  Extracting ... "
  local tmp_x="$TMPDIR/_coconut_extract_$$"
  mkdir -p "$tmp_x"
  if has_cmd unzip; then unzip -qo "$tmp_zip" -d "$tmp_x"
  else busybox unzip -qo "$tmp_zip" -d "$tmp_x" 2>/dev/null || err "Need unzip."; fi
  rm -f "$tmp_zip"

  local extracted
  extracted=$(find "$tmp_x" -type f -name "$inner" 2>/dev/null | head -1)
  [ -n "$extracted" ] || extracted=$(find "$tmp_x" -type f 2>/dev/null | head -1)
  [ -n "$extracted" ] || err "Binary not found in archive."
  echo "done"

  printf "  Installing coconut ... "
  cp "$extracted" "$BIN_DIR/$final"
  chmod +x "$BIN_DIR/$final"
  rm -rf "$tmp_x"
  echo "done"

  printf "  Installing coconut-cli ... "
  local cli_archive="coconut-cli-$PLATFORM-$ARCH.zip"
  local cli_inner="coconut-cli"
  [ "$PLATFORM" = "windows" ] && cli_inner="coconut-cli.exe"
  local cli_url="https://github.com/$REPO/releases/download/$VERSION/$cli_archive"
  local tmp_cli="$TMPDIR/_coconut_cli_$$.zip"
  download "$cli_url" "$tmp_cli" || err "download failed: $cli_archive"
  local tmp_cx="$TMPDIR/_coconut_cli_extract_$$"
  mkdir -p "$tmp_cx"
  if has_cmd unzip; then unzip -qo "$tmp_cli" -d "$tmp_cx"
  else busybox unzip -qo "$tmp_cli" -d "$tmp_cx" 2>/dev/null || err "Need unzip."; fi
  rm -f "$tmp_cli"
  local cli_bin
  cli_bin=$(find "$tmp_cx" -type f -name "$cli_inner" 2>/dev/null | head -1)
  [ -n "$cli_bin" ] || cli_bin=$(find "$tmp_cx" -type f 2>/dev/null | head -1)
  [ -n "$cli_bin" ] || err "coconut-cli not found in archive."
  cp "$cli_bin" "$BIN_DIR/$cli_inner"
  chmod +x "$BIN_DIR/$cli_inner"
  rm -rf "$tmp_cx"
  echo "done"

  printf "  Installing create-coconut-app + schemas + skill into %s ... " "$COCONUT_HOME"
  local tools_archive="coconut-tools-$VERSION.zip"
  local tools_url="https://github.com/$REPO/releases/download/$VERSION/$tools_archive"
  local tmp_tools="$TMPDIR/_coconut_tools_$$.zip"
  download "$tools_url" "$tmp_tools" || err "download failed: $tools_archive"
  local tmp_tx="$TMPDIR/_coconut_tools_extract_$$"
  mkdir -p "$tmp_tx"
  if has_cmd unzip; then unzip -qo "$tmp_tools" -d "$tmp_tx"
  else busybox unzip -qo "$tmp_tools" -d "$tmp_tx" 2>/dev/null || err "Need unzip."; fi
  rm -f "$tmp_tools"
  # Install the whole tools bundle into COCONUT_HOME (the create script's
  # home). The create script resolves argparse.lua next to its real path and
  # schemas/skill from $COCONUT_HOME/{schemas,skill}, so this layout makes
  # everything work offline with no extra copies.
  mkdir -p "$COCONUT_HOME"
  cp -r "$tmp_tx/tools/." "$COCONUT_HOME/"
  rm -rf "$tmp_tx"
  # Symlink the script into the bin dir so it's on PATH (single source of
  # truth in $COCONUT_HOME; readlink -f resolves the real path).
  ln -sf "$COCONUT_HOME/create-coconut-app" "$BIN_DIR/create-coconut-app"
  echo "done"

  printf '%s\n' "$VERSION" > "$COCONUT_HOME/VERSION"

  printf '\n'
  info "Installed coconut $VERSION -> $BIN_DIR/$final"
  info "Installed coconut-cli -> $BIN_DIR/$cli_inner"
  info "Installed create-coconut-app -> $BIN_DIR/create-coconut-app (tools in $COCONUT_HOME)"
  if ! has_cmd luajit && ! has_cmd lua; then
    warn "create-coconut-app needs LuaJIT (or lua) on PATH."
  fi

  header "> Add to PATH"
  case ":$PATH:" in
    *":$BIN_DIR:"*) info "Already on PATH" ;;
    *) echo "  export PATH=\"$BIN_DIR:\$PATH\"" ;;
  esac
  printf '\n%b%s%b\n' "$GREEN" "Done — happy building!" "$RESET"
}

# ── Explain + confirm (offline, BEFORE any network) ──────────────────────────

print_banner
printf '%b\n' "${LAVENDER}Coconut Milk — a Lua-first desktop UI framework.${RESET}"
printf '%s\n' ""
printf '%s\n' "  This installer will:"
printf '%b\n' "    1. Download the latest Coconut Milk release binary for your platform${RESET}"
printf '%b\n' "    2. Install it as ${BOLD}coconut${RESET} into $BIN_DIR${RESET}"
printf '%b\n' "    3. Install the ${BOLD}coconut-cli${RESET} generator CLI into $BIN_DIR${RESET}"
printf '%b\n' "    4. Install the ${BOLD}create-coconut-app${RESET} scaffolding script into $BIN_DIR${RESET}"
printf '%b\n' "    (and the full tools bundle — script, vendored argparse, schemas, agent skill — into $COCONUT_HOME)${RESET}"
printf '%s\n' ""

if [ "$DRY_RUN" = "1" ]; then
  detect_platform
  echo "  Plan:"
  echo "    Platform : $PLATFORM ($ARCH)"
  echo "    Version  : ${REQUESTED_VERSION:-latest}"
  echo "    Binaries : $BIN_DIR"
  echo "    Assets   : $COCONUT_HOME"
  echo "    Would download:"
  echo "      https://github.com/$REPO/releases/download/<version>/coconut-$PLATFORM-$ARCH.zip"
  echo "      https://github.com/$REPO/releases/download/<version>/coconut-cli-$PLATFORM-$ARCH.zip"
  echo "      https://github.com/$REPO/releases/download/<version>/coconut-tools-<version>.zip"
  exit 0
fi

confirm() {
  [ "$YES" = "1" ] && return 0
  [ -r /dev/tty ] || err "no interactive terminal; use --yes to confirm"
  printf '%b' "${YELLOW}Continue with installation? [Y/n] ${RESET}"
  read -r answer </dev/tty || answer=
  case "$answer" in
    y|Y|yes|YES) return 0 ;;
    n|N|no|NO) return 1 ;;
    '') return 0 ;;
    *) return 1 ;;
  esac
}

confirm || { printf '%s\n' "Installation cancelled."; exit 0; }

do_install
exit 0
