#!/bin/bash
# 烧录固件 + 打开串口监视
# 用法：./tools/flash.sh [port]

PORT=${1:-/dev/ttyACM0}

set -e
cd "$(dirname "$0")/.."

echo "=== 编译 ==="
idf.py build

echo "=== 烧录 ($PORT) ==="
idf.py -p "$PORT" flash

echo "=== 监视 ==="
idf.py -p "$PORT" monitor
