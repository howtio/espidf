#!/bin/bash
# 仅打开串口监视（不编译、不烧录）
# 用法：./tools/monitor.sh [port]

PORT=${1:-/dev/ttyACM0}

cd "$(dirname "$0")/.."
idf.py -p "$PORT" monitor
