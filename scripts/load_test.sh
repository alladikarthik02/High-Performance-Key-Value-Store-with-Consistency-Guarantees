#!/usr/bin/env bash
set -euo pipefail
./build/benchmark --endpoint 127.0.0.1:5001 \
                  --threads 16 --duration 30 \
                  --pipeline 32 --value_size 128
