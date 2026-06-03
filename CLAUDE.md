# ESP32 MP3 Player — Claude Code 自动加载规则

> 硬件：ESP32-S3-Touch-AMOLED-1.8 | 开发方式：ESP-IDF CLI / idf.py

---

## 项目身份

**esp32-mp3-player** = 触摸屏 MP3 播放器，带电子宠物 GIF 动画 + 系统状态监控。

```text
SD 卡 (MP3) → ESP32-S3 → I2S 音频输出 + LCD 显示 + 触摸交互
                          + USB Serial 调试日志
```

---

## 架构铁律

```
1. 音频优先 — PCM RingBuffer 低于阈值时，GIF/UI 必须降级，音频绝不能断
2. PSRAM 大水库，SRAM DMA 小水槽 — 大缓冲放 PSRAM，DMA 直接用的放 SRAM
3. 模块间只走 Queue + Event，不直接互调 — Touch 不发音频命令，Audio 不刷屏
4. 单一状态机 — App FSM 是所有控制流的唯一裁决者
5. 运行时不 malloc/free — 大缓冲启动时一次性申请
6. 串口即眼睛 — 每个模块初始化/状态变更/异常都必须打串口日志
```

---

## 文件结构

```text
esp32-mp3-player/
├── main/                    ← ESP-IDF 主入口
│   ├── main.cpp
│   └── CMakeLists.txt
├── components/              ← 解耦模块（各自 CMakeLists.txt）
│   ├── audio_player/        ← 播放控制 + 水位监控
│   ├── mp3_decoder/         ← MP3 → PCM
│   ├── storage_manager/     ← SD 卡挂载 + 文件扫描
│   ├── display_manager/     ← LCD + LVGL 驱动
│   ├── gif_player/          ← GIF 解码 + 帧率/降级
│   ├── touch_manager/       ← I2C 触摸坐标
│   ├── button_manager/      ← GPIO 按键消抖
│   ├── playlist/            ← 歌曲列表 + 索引
│   ├── memory_pool/         ← 统一 SRAM/PSRAM 分配
│   └── system_monitor/      ← 系统状态采集 + 串口日志 + 屏幕状态栏
├── assets/                  ← 内置 GIF + 字体
│   └── processed/           ← 处理后的资源 (SD 卡用)
├── config/                  ← 硬件配置头文件
├── docs/                    ← 施工文档 (00~09)
├── tools/                   ← flash / monitor / sd_setup 脚本
├── CMakeLists.txt           ← 顶层 CMake
├── sdkconfig.defaults       ← ESP-IDF 默认配置
└── partitions.csv           ← 分区表
```

---

## 开发 SOP

> 详见 `docs/02_开发流程.md`

```text
开工（逐项打勾）：
□ 1. 读 docs/00 + 01 + 04 + DEV_PROGRESS + LOG + 09
□ 2. git status && git branch（确认在 main，工作区干净）
□ 3. git checkout -b backup/<YYYYMMDD>-xxx && git push -u origin backup/<YYYYMMDD>-xxx && git checkout main
□ 4. 列出开发计划 → 等用户确认 → 再动手写代码
□ 5. idf.py build（确认起点可编译）

收尾（逐项打勾，缺一不可）：
□ 1. idf.py build（必须通过）
□ 2. 确认串口日志全覆盖（用 09 的检查清单）
□ 3. 更新 docs/LOG.md（必须追加）+ docs/DEV_PROGRESS.md（必须更新）+ 00/01/06/09（按需）
□ 4. git add <具体文件> && git commit（禁止 git add -A）
□ 5. git push origin main
□ 6. 输出：编译结果、提交号、备份分支、回滚判断
```

---

## Git

| 项目 | 值 |
|------|-----|
| 仓库 | `git@github.com:howtio/espidf.git` |
| 默认分支 | `main` |
| 备份分支 | `backup/<YYYYMMDD>-<描述>` |
| 功能分支 | `feat/<描述>` |
| 不进 Git | `build/` `sdkconfig` `*.bin` `*.elf` `*.map` |

**回滚顺序：** 文件级 → 提交级 → 分支级

---

## 调试铁律

> 详见 `docs/09_调试与串口打印规范.md`

```
1. Claude 每写一段代码，必须带串口日志打印
2. 模块初始化 → LOG_INF，失败 → LOG_ERR
3. 状态变更 → LOG_INF，异常 → LOG_WRN/LOG_ERR
4. 每 5 秒打印一次: 内存/电池/PCM水位/GIF帧率
5. 启动时打印一次全景 (所有硬件初始化结果)
6. 如果 idf.py monitor 看不到对应日志 → 调试基建没跑通，不算完成
```

---

## 文档索引

| 读什么 | 文件 |
|--------|------|
| 项目总览 | `docs/00_总体施工文档.md` |
| 架构契约 | `docs/01_架构说明.md` |
| 开发 SOP | `docs/02_开发流程.md` |
| 硬件引脚 | `docs/03_硬件引脚分配.md` |
| 开发计划 | `docs/04_开发计划.md` |
| 内存预算 | `docs/05_内存预算.md` |
| 模块接口 | `docs/06_模块接口规范.md` |
| 架构全景图 | `docs/07_架构全景图.md` |
| 硬件数据交互 | `docs/08_硬件数据交互图.md` |
| 调试规范 | `docs/09_调试与串口打印规范.md` |
| 施工日志 | `docs/LOG.md` |
| 开发进度 | `docs/DEV_PROGRESS.md` |

---

## 安全红线

- 绝对禁止提交 `sdkconfig` 到 Git（含本地路径和密钥）
- `build/` 不进仓库
- 不在代码里硬编码 WiFi 密码或 API Key
- 不在注释中写默认密码
