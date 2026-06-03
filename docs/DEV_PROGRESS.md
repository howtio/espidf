# 开发进度

> 当前进度、下一步计划、遗留问题。每次收尾更新。

---

## 当前版本：0.1.0

## 当前 Phase：Phase 1 — 基础可用

## 已完成

- [x] 项目目录骨架（main + 10 components）
- [x] 10 份施工文档（00~09 + LOG + DEV_PROGRESS）
- [x] 板级配置头文件 `config/board_config.h`
- [x] ESP-IDF sdkconfig.defaults（含 PSRAM/FreeRTOS/I2S/SDMMC）
- [x] 分区表 `partitions.csv`
- [x] 顶层 CMakeLists.txt + main/CMakeLists.txt + 10 component CMakeLists.txt
- [x] CLAUDE.md 开发规则（含调试铁律）
- [x] .gitignore + README.md
- [x] 10 个模块的接口规范文档（含 SystemMonitor）
- [x] 调试与串口打印规范 `docs/09_调试与串口打印规范.md`
- [x] GIF 处理：720×720 → 160×160, 10fps, 407KB
- [x] MP3 处理：128kbps 立体声 → 64kbps mono 22050Hz
- [x] SD 卡初始化脚本 `tools/setup_sd_card.sh`
- [x] 架构全景图 + 硬件数据交互图

## 下一步（Phase 1 收尾 → Phase 2 开始）

1. 创建 GitHub 仓库 `git@github.com:howtio/espidf.git`
2. 首次提交推送
3. `idf.py build` 验证编译链路
4. `idf.py monitor` 看到启动 Banner
5. 进入 Phase 2：I2C 扫描 → IO 扩展器 → LCD 点亮 → 触摸坐标 + 状态栏占位

## 遗留问题

- [ ] GitHub 仓库尚未创建
- [ ] 首次编译未验证（需要 ESP-IDF 环境）
- [ ] SD 卡初始化脚本待执行（sudo 权限）
- [ ] 10 component 的 .hpp/.cpp 源码待实现

## 文档列表（10 份）

```
docs/
├── 00_总体施工文档.md          ✅ 含 UI 布局 + 状态栏
├── 01_架构说明.md              ✅ 含 SystemMonitor + 日志架构
├── 02_开发流程.md              ✅ 开发 SOP + 回滚
├── 03_硬件引脚分配.md          ✅ GPIO + I2C + 初始化顺序
├── 04_开发计划.md              ✅ 7 Phase + 每阶段调试日志要求
├── 05_内存预算.md              ✅ PSRAM/SRAM + 日志缓冲
├── 06_模块接口规范.md          ✅ 10 模块 .hpp 接口
├── 07_架构全景图.md            ✅ 完整架构图
├── 08_硬件数据交互图.md        ✅ CPU/DMA/PSRAM/SRAM/SD/I2S/LCD/I2C
├── 09_调试与串口打印规范.md    ✅ 🆕 调试规范
├── LOG.md                     ✅ 施工日志
└── DEV_PROGRESS.md            ✅ 你在这
```
