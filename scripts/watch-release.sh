#!/usr/bin/env bash
# =========================================================================
# watch-release.sh
#
# Cron-friendly script: checks the latest GitHub Actions workflow run for
# the v0.1.0.alpha-1 tag every N minutes.  On completion it:
#   1. Notifies this Pi agent via pi-notify
#   2. Removes the cron job (self-unregister)
#
# Usage:
#   ./scripts/watch-release.sh           # one-shot check (cron calls this)
#   ./scripts/watch-release.sh install   # register the cron job
#   ./scripts/watch-release.sh uninstall # remove the cron job
# =========================================================================
TAG="v0.1.0.alpha-1"
WORKFLOW="build.yml"
POLL_MINUTES=2

# Pi agent to notify — this agent (the one you're talking to right now)
PI_AGENT_ID="coconut-milk"
# =========================================================================

set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
CRON_EXPR="*/$POLL_MINUTES * * * *"
CRON_JOB="$CRON_EXPR cd $REPO_ROOT && $SCRIPT_DIR/watch-release.sh >> /tmp/coconut-watch.log 2>&1"

check_deps() {
  if ! command -v gh &>/dev/null; then
    echo "ERROR: gh not found"
    exit 1
  fi
}

get_run_id() {
  gh run list \
    --workflow "$WORKFLOW" \
    --branch "$TAG" \
    --limit 1 \
    --json databaseId,status,conclusion \
    --jq '.[0] // empty' 2>/dev/null
}

send_pi_notify() {
  local status="$1"
  local summary="$2"
  local detail="$3"
  if [ -z "$PI_AGENT_ID" ]; then
    echo "  [skip] pi-notify: PI_AGENT_ID not set"
    return
  fi
  echo "  pi-notify -> agent-id=$PI_AGENT_ID type=build_result title=\"$summary\""
  pi-notify \
    -agent-id "$PI_AGENT_ID" \
    -type "build_result" \
    -title "$summary" \
    -m "$detail" \
    -timeout 5000
  local rc=$?
  if [ $rc -eq 0 ]; then
    echo "  ✓ pi-notify delivered"
  else
    echo "  ✗ pi-notify failed (exit $rc)"
  fi
  return $rc
}

remove_cron() {
  local reason="$1"
  echo "Removing cron job ($reason)..."
  crontab -l 2>/dev/null \
    | grep -v "watch-release.sh" \
    | crontab -
  echo "  cron unregistered"
}

install_cron() {
  if crontab -l 2>/dev/null | grep -q "watch-release.sh"; then
    echo "Cron job already installed."
    crontab -l | grep "watch-release.sh"
    exit 0
  fi
  (crontab -l 2>/dev/null; echo "$CRON_JOB") | crontab -
  echo "Installed cron job:"
  echo "  $CRON_JOB"
  echo "Logs: tail -f /tmp/coconut-watch.log"
}

uninstall_cron() {
  remove_cron "manual uninstall"
  echo "Cron uninstalled."
}

# ── Main ──────────────────────────────────────────────────────────────

case "${1:-}" in
  install)
    install_cron
    exit 0
    ;;
  uninstall)
    uninstall_cron
    exit 0
    ;;
esac

check_deps

TIMESTAMP=$(date '+%Y-%m-%d %H:%M:%S')
echo "[$TIMESTAMP] Checking build status for tag $TAG..."

RUN_JSON=$(get_run_id)

if [ -z "$RUN_JSON" ]; then
  echo "  No workflow run found for tag $TAG (still pending or not triggered yet)"
  exit 0
fi

RUN_ID=$(echo "$RUN_JSON" | jq -r '.databaseId // "unknown"')
STATUS=$(echo "$RUN_JSON"  | jq -r '.status // "unknown"')
CONCLUSION=$(echo "$RUN_JSON" | jq -r '.conclusion // ""')

echo "  Run ID: $RUN_ID"
echo "  Status: $STATUS"
echo "  Conclusion: ${CONCLUSION:-"(none yet)"}"

# ── Still running → wait for next cron tick ─────────────────────────
if [ "$STATUS" = "in_progress" ] || [ "$STATUS" = "queued" ] || [ "$STATUS" = "requested" ] || [ "$STATUS" = "waiting" ]; then
  echo "  Build still in progress — will check again."
  exit 0
fi

# ── Completed ─────────────────────────────────────────────────────────
if [ "$STATUS" = "completed" ]; then
  if [ "$CONCLUSION" = "success" ]; then
    SUMMARY="✅ Build succeeded for $TAG"
    DETAIL="All jobs passed for tag $TAG. Release is ready at: https://github.com/lakubuDavid/coconut-milk/releases/tag/$TAG"

    echo "  ✅ Build succeeded!"
    send_pi_notify "success" "$SUMMARY" "$DETAIL"
    remove_cron "build succeeded"
    exit 0

  elif [ "$CONCLUSION" = "failure" ] || [ "$CONCLUSION" = "cancelled" ]; then
    SUMMARY="❌ Build failed for $TAG"
    DETAIL="Build failed for tag $TAG. Check: https://github.com/lakubuDavid/coconut-milk/actions/runs/$RUN_ID"

    echo "  ❌ Build failed!"
    send_pi_notify "failure" "$SUMMARY" "$DETAIL"
    remove_cron "build failed"
    exit 1

  else
    SUMMARY="⚠️ Build $CONCLUSION for $TAG"
    DETAIL="Build conclusion is '$CONCLUSION' for tag $TAG. Run: https://github.com/lakubuDavid/coconut-milk/actions/runs/$RUN_ID"

    echo "  ⚠️ Build $CONCLUSION"
    send_pi_notify "$CONCLUSION" "$SUMMARY" "$DETAIL"
    remove_cron "build $CONCLUSION"
    exit 0
  fi
fi

# Unknown status — keep cron alive
echo "  Unknown status '$STATUS' — will check again."
exit 0
