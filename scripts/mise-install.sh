#!/usr/bin/env bash
# mise-install.sh — symlink the built coconut binary + create-coconut-app
# into ~/.local/bin (override with COCONUT_INSTALL_DIR). Called by
# `mise run install` / `mise run install-release` (the mode was configured
# by the task's depends, so `xmake show` reports the right targetfile).

set -euo pipefail

INSTALL_DIR="${COCONUT_INSTALL_DIR:-${HOME}/.local/bin}"
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

mkdir -p "${INSTALL_DIR}"

# Populate ~/.coconut with bundled assets (schemas + agent skill) so that
# create-coconut-app works fully offline, independent of CWD or the repo.
COCONUT_HOME="${COCONUT_HOME:-${HOME}/.coconut}"
mkdir -p "${COCONUT_HOME}/schemas" "${COCONUT_HOME}/skill"
cp -f "${PROJECT_ROOT}/schemas/coconut.d.lua" "${COCONUT_HOME}/schemas/" 2>/dev/null || true
cp -f "${PROJECT_ROOT}/schemas/coconut.d.ts" "${COCONUT_HOME}/schemas/" 2>/dev/null || true
cp -f "${PROJECT_ROOT}/SKILL.md" "${COCONUT_HOME}/skill/SKILL.md" 2>/dev/null || true

# Resolve the actual binary path (strip ANSI codes from `xmake show` output).
# The coconut target lives in apps/coconut, so xmake must run from there —
# there is no root xmake.lua.
APP_DIR="${PROJECT_ROOT}/apps/coconut"
BIN="$(cd "${APP_DIR}" && xmake show -t coconut 2>/dev/null \
  | awk -F': ' '{gsub(/\033\[[0-9;]*m/,"",$0); if ($1 ~ /targetfile$/) { printf "%s/%s", "'"${APP_DIR}"'", $2; print "" }}')"

if [ -z "${BIN}" ] || [ ! -f "${BIN}" ]; then
  echo "error: could not resolve the coconut binary — run 'mise run build' first" >&2
  exit 1
fi

chmod +x "${BIN}"
ln -sf "${BIN}" "${INSTALL_DIR}/coconut"
ln -sf "${PROJECT_ROOT}/scripts/create-coconut-app" "${INSTALL_DIR}/create-coconut-app"

echo "installed to ${INSTALL_DIR}:"
ls -la "${INSTALL_DIR}/coconut" "${INSTALL_DIR}/create-coconut-app"
