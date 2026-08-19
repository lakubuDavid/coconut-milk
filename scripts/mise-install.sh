#!/usr/bin/env bash
# mise-install.sh — symlink the built coconut binary + create-coconut-app
# into ~/tools. Called by `mise run install` / `mise run install-release`
# (the mode was configured by the task's depends, so `xmake show`
# reports the right targetfile).
#
# Mirror of the Justfile `install` target.

set -euo pipefail

INSTALL_DIR="${HOME}/tools"
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

mkdir -p "${INSTALL_DIR}"

# Resolve the actual binary path the way Justfile's _targetfile does
# (strip ANSI codes from `xmake show` output).
BIN="$(cd "${PROJECT_ROOT}" && xmake show -t coconut 2>/dev/null \
  | awk -F': ' '{gsub(/\033\[[0-9;]*m/,"",$0); if ($1 ~ /targetfile$/) { printf "%s/%s", "'"${PROJECT_ROOT}"'", $2; print "" }}')"

if [ -z "${BIN}" ] || [ ! -f "${BIN}" ]; then
  echo "error: could not resolve the coconut binary — run 'mise run build' first" >&2
  exit 1
fi

chmod +x "${BIN}"
ln -sf "${BIN}" "${INSTALL_DIR}/coconut"
ln -sf "${PROJECT_ROOT}/scripts/create-coconut-app" "${INSTALL_DIR}/create-coconut-app"

echo "installed to ${INSTALL_DIR}:"
ls -la "${INSTALL_DIR}/coconut" "${INSTALL_DIR}/create-coconut-app"
