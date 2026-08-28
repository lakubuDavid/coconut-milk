#!/usr/bin/env bash
# mise-install-cli.sh — symlink the built coconut-cli binary + create-coconut-app
# into ~/.local/bin (override with COCONUT_INSTALL_DIR). Called by
# `mise run install` / `mise run install-release` in apps/coconut-cli (the
# build step is provided by the task's depends, so xmake is already configured).
#
# Note: the coconut-cli target is defined under apps/coconut-cli, so xmake must
# run from there to resolve the targetfile.

set -euo pipefail

INSTALL_DIR="${COCONUT_INSTALL_DIR:-${HOME}/.local/bin}"
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CLI_DIR="${PROJECT_ROOT}/apps/coconut-cli"

mkdir -p "${INSTALL_DIR}"

# Resolve the actual coconut-cli binary path (strip ANSI codes from xmake output).
BIN="$(cd "${CLI_DIR}" && xmake show -t coconut-cli 2>/dev/null \
  | awk -F': ' '{gsub(/\033\[[0-9;]*m/,"",$0); if ($1 ~ /targetfile$/) { printf "%s/%s", "'"${CLI_DIR}"'", $2; print "" }}')"

if [ -z "${BIN}" ] || [ ! -f "${BIN}" ]; then
  echo "error: could not resolve the coconut-cli binary — run 'mise run build' first" >&2
  exit 1
fi

chmod +x "${BIN}"
ln -sf "${BIN}" "${INSTALL_DIR}/coconut-cli"
ln -sf "${PROJECT_ROOT}/scripts/create-coconut-app" "${INSTALL_DIR}/create-coconut-app"

echo "installed to ${INSTALL_DIR}:"
ls -la "${INSTALL_DIR}/coconut-cli" "${INSTALL_DIR}/create-coconut-app"
