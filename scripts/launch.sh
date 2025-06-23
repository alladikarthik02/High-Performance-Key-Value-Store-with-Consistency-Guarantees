#!/usr/bin/env bash
set -euo pipefail
killall -q kv_server || true

peers="127.0.0.1:5001,127.0.0.1:5002,127.0.0.1:5003"

for id in 1 2 3; do
  ID=$id PEERS=$peers DATA=/tmp/node$id \
    ./build/kv_server > /tmp/node$id.log 2>&1 &
done

echo "Launched 3 nodes. Tail logs: tail -f /tmp/node1.log"
