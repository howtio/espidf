# ESP32 MP3 Player

触摸屏 MP3 播放器，带电子宠物 GIF 动画。基于 ESP32-S3-Touch-AMOLED-1.8。

```text
SD 卡 (MP3) → ESP32-S3 → I2S 音频 + LCD 电子宠物 + 触摸交互
```

## 硬件

- 主控：ESP32-S3R8（双核 LX7, 240MHz, 8MB PSRAM, 16MB Flash）
- 屏幕：1.8" AMOLED 368×448, QSPI
- 触摸：FT3168, I2C
- 音频：ES8311 Codec, I2S
- 存储：microSD, SDMMC 1-bit

## 快速开始

```bash
# 设置 ESP-IDF 环境
. ~/esp/esp-idf/export.sh

# 编译
idf.py build

# 烧录 + 监视
idf.py flash monitor
```

## 结构

```text
main/         ← 主入口 + App FSM
components/   ← 解耦模块（audio/display/gif/touch/storage/playlist）
assets/       ← 内置 GIF + 字体资源
config/       ← 硬件配置头文件
docs/         ← 施工文档
tools/        ← 辅助脚本
```

## 文档

| | |
|---|---|
| [00_总体施工文档](docs/00_总体施工文档.md) | 项目总览 |
| [01_架构说明](docs/01_架构说明.md) | 架构契约 |
| [02_开发流程](docs/02_开发流程.md) | 开发 SOP |
| [03_硬件引脚分配](docs/03_硬件引脚分配.md) | 硬件引脚参考 |
| [04_开发计划](docs/04_开发计划.md) | 分阶段计划 |
| [06_模块接口规范](docs/06_模块接口规范.md) | 解耦接口定义 |
