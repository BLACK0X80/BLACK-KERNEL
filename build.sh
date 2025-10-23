#!/usr/bin/env bash
set -euo pipefail
mkdir -p build
make -j$(nproc)

