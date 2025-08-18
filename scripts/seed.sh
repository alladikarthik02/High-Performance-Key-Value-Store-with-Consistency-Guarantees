# scripts/seed.sh
#!/usr/bin/env bash
set -euo pipefail

SVC="${1:-kv1}"          # compose service/container name
ADDR="${2:-kv1:50051}"   # server address kv1:50051 etc
N="${3:-50}"             # how many keys

for i in $(seq 0 $((N-1))); do
  docker compose exec -T "$SVC" kv_client "$ADDR" put "user:$i" "val-$i" >/dev/null || true
done
echo "Seeded $N keys to $ADDR"
