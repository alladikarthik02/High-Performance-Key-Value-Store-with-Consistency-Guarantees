# scripts/kvctl.sh
#!/usr/bin/env bash
set -euo pipefail

SVC="${SVC:-kv1}"          # container/service to exec into
ADDR="${ADDR:-kv1:50051}"  # target address
OP="${1:-help}"

case "$OP" in
  put)
    KEY="${2:?key}"; VAL="${3:?value}"
    docker compose exec -T "$SVC" kv_client "$ADDR" put "$KEY" "$VAL"
    ;;
  get)
    KEY="${2:?key}"
    docker compose exec -T "$SVC" kv_client "$ADDR" get "$KEY"
    ;;
  del|delete)
    KEY="${2:?key}"
    docker compose exec -T "$SVC" kv_client "$ADDR" delete "$KEY"
    ;;
  scan-prefix)
    PFX="${2:?prefix}"
    docker compose exec -T "$SVC" kv_client "$ADDR" scan-prefix "$PFX"
    ;;
  scan-range)
    START="${2:?start}"; END="${3:?end}"
    docker compose exec -T "$SVC" kv_client "$ADDR" scan-range "$START" "$END"
    ;;
  *)
    echo "Usage:"
    echo "  SVC=kv1 ADDR=kv1:50051 scripts/kvctl.sh put <key> <value>"
    echo "  SVC=kv1 ADDR=kv1:50051 scripts/kvctl.sh get <key>"
    echo "  SVC=kv1 ADDR=kv1:50051 scripts/kvctl.sh delete <key>"
    echo "  SVC=kv1 ADDR=kv1:50051 scripts/kvctl.sh scan-prefix <prefix>"
    echo "  SVC=kv1 ADDR=kv1:50051 scripts/kvctl.sh scan-range <start> <end>"
    ;;
esac
