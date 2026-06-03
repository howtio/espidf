#!/bin/bash
# SD 卡初始化脚本 — 格式化 + 复制资源文件
# 用法：sudo ./tools/setup_sd_card.sh
# 警告：此脚本会清空 /dev/sda 全部数据！

set -e

SD_DEV="/dev/sda"
SD_PART="${SD_DEV}1"
MOUNT_POINT="/mnt/sdcard"
ASSETS_DIR="$(cd "$(dirname "$0")/.." && pwd)/assets/processed"

echo "============================================"
echo "  ESP32 MP3 Player — SD 卡初始化"
echo "============================================"
echo ""
echo "目标设备: ${SD_DEV}"
echo "危险操作: 将清空 ${SD_DEV} 全部数据！"
echo ""
read -p "确认继续？(输入 yes 继续): " confirm
if [ "$confirm" != "yes" ]; then
    echo "已取消"
    exit 0
fi

# Step 1: 卸载已有分区
echo ""
echo "=== Step 1: 卸载已有分区 ==="
for part in ${SD_DEV}*; do
    if mount | grep -q "$part"; then
        umount "$part" 2>/dev/null || true
        echo "  已卸载 $part"
    fi
done
echo "  完成"

# Step 2: 创建分区表 + 单个 FAT32 分区
echo ""
echo "=== Step 2: 创建分区表 ==="
wipefs -a "$SD_DEV"
parted -s "$SD_DEV" mklabel msdos
parted -s "$SD_DEV" mkpart primary fat32 0% 100%
echo "  分区表已创建: MBR + 单个 FAT32 分区"

# Step 3: 格式化 FAT32
echo ""
echo "=== Step 3: 格式化 FAT32 ==="
mkfs.vfat -F 32 -n "ESP32MP3" "${SD_PART}"
echo "  格式化完成: LABEL=ESP32MP3"

# Step 4: 挂载
echo ""
echo "=== Step 4: 挂载 ==="
mkdir -p "$MOUNT_POINT"
mount "${SD_PART}" "$MOUNT_POINT"
echo "  已挂载到 ${MOUNT_POINT}"

# Step 5: 创建目录结构
echo ""
echo "=== Step 5: 创建目录结构 ==="
mkdir -p "${MOUNT_POINT}/music"
mkdir -p "${MOUNT_POINT}/pet"
echo "  music/ — MP3 歌曲文件"
echo "  pet/   — 电子宠物 GIF + 原始帧"

# Step 6: 复制 MP3 歌曲
echo ""
echo "=== Step 6: 复制 MP3 歌曲 ==="
for f in "${ASSETS_DIR}/songs/"*.mp3; do
    [ -f "$f" ] || continue
    name=$(basename "$f")
    cp "$f" "${MOUNT_POINT}/music/${name}"
    size=$(stat --printf='%s' "$f")
    echo "  ${name}  ($((size/1024)) KB)"
done

# Step 7: 复制 GIF + 原始帧
echo ""
echo "=== Step 7: 复制 GIF 资源 ==="
cp "${ASSETS_DIR}/ui_160x160.gif" "${MOUNT_POINT}/pet/"
echo "  ui_160x160.gif  ($(stat --printf='%s' "${ASSETS_DIR}/ui_160x160.gif" | awk '{print int($1/1024)}') KB)"

mkdir -p "${MOUNT_POINT}/pet/frames"
for f in "${ASSETS_DIR}/frames/"*.raw; do
    [ -f "$f" ] || continue
    cp "$f" "${MOUNT_POINT}/pet/frames/"
done
frame_count=$(ls "${ASSETS_DIR}/frames/"*.raw 2>/dev/null | wc -l)
echo "  frames/ — ${frame_count} 帧 RGB565 原始数据"

# Step 8: 同步 + 卸载
echo ""
echo "=== Step 8: 同步 + 卸载 ==="
sync
df -h "${MOUNT_POINT}"
umount "${MOUNT_POINT}"
echo "  已安全卸载"

# Step 9: 总结
echo ""
echo "============================================"
echo "  SD 卡初始化完成！"
echo "============================================"
echo ""
echo "SD 卡分区: ${SD_PART} (FAT32, LABEL=ESP32MP3)"
echo ""
echo "文件结构:"
echo "  /music/"
echo "    01.mp3  (2128 KB, mono 22050Hz 64kbps)"
echo "    02.mp3  (1426 KB, mono 22050Hz 64kbps)"
echo "    03.mp3  (2048 KB, mono 22050Hz 64kbps)"
echo "    04.mp3  (2006 KB, mono 22050Hz 64kbps)"
echo "    05.mp3  (1290 KB, mono 22050Hz 64kbps)"
echo ""
echo "  /pet/"
echo "    ui_160x160.gif   (407 KB, 160×160, 10fps, 71帧)"
echo "    frames/          (71 × 160×160 RGB565 raw, 50KB/帧)"
echo ""
echo "总大小: $(du -sh "${ASSETS_DIR}" 2>/dev/null | awk '{print $1}')"
