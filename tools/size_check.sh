#!/bin/bash
# 检查固件和组件大小
# 用法：./tools/size_check.sh

set -e
cd "$(dirname "$0")/.."

echo "=== 编译 ==="
idf.py build

echo ""
echo "=== 固件总大小 ==="
idf.py size

echo ""
echo "=== 各组件大小 ==="
idf.py size-components
