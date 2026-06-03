# 开发进度

> 当前进度、下一步计划、遗留问题。每次收尾更新。

---

## 当前版本：0.1.0

## 当前 Phase：Phase 3 — 音频保活下的显示骨架落地

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
- [x] `idf.py build` 编译链路已验证通过（2026-06-04）
- [x] `main` 启动链路已收敛为：I2C 扫描 → LCD → 触摸 → SD → Audio → 周期状态日志
- [x] `audio_player` 已改为流式解码播放，避免整文件解码导致的内存和栈风险
- [x] 实机确认当前 SD 资源实际为 `16000Hz mono`，输出链路已同步改为 `16000Hz`
- [x] ES8311/I2S 初始化已按官方 `i2s_es8311` 示例对齐：`384x MCLK`、显式 `sample_frequency_config`、显式 `unmute`
- [x] 串口实机监看已确认系统可连续播放并持续输出 `Heartbeat` / `Streaming frame` 日志，未复现音频链路触发的重启
- [x] 已新增显式 `test-tone` 启动模式，实机完成 `1kHz / 2s` I2S + ES8311 基线验证
- [x] 音频链路已切到 `PSRAM MP3ReadBuf + SRAM PCMOutBuf`，播放期间不再临时 `malloc/free`
- [x] `PCM RingBuf(PSRAM)` 已落地，当前实机可打印真实 `PCM water=%` 水位日志
- [x] 启动后已支持测试音 A/B 后进入连续 MP3 播放，并具备自动下一首/循环回第一首的最小闭环
- [x] `memory_pool` / `system_monitor` 已补最小可用实现，可打印内存与电源占位日志
- [x] `display_manager` 已从“测试线”升级为最小稳定 UI：顶部状态栏、GIF 占位区、歌曲信息区、PCM 水位条、控制按钮区
- [x] 显示链路已按 `PSRAM Framebuffer + SRAM FlushBuf + QSPI DMA` 结构运行，实机串口已确认局刷日志稳定输出
- [x] 屏幕刷新已接入真实歌曲名和周期状态栏更新，同时未打断现有音频播放链路
- [x] 触摸轮询已从 `5s` 级别改成 `20ms` UI 节拍，实机串口已收到真实触摸坐标日志
- [x] 底部控件已切成传统播放器样式：`|<< / || / >>|`，并接入 `pause / next / prev` 最小控制通路
- [x] 进度条已从 `PCM BUFFER` 升级为歌曲进度近似值显示
- [x] 触摸逻辑已从“按下即触发”改为“按下-释放在同一控件才触发”，增加消抖和滑动高亮追踪
- [x] `display_manager` 已引入 FreeRTOS Mutex 保护 framebuffer 并发写
- [x] UI 刷新节拍已分拆为：进度条 250ms、控件 500ms、状态栏 5s，减少触摸冲突

## 下一步
1. 把真实 GIF 源 `/home/howtion/biliesp/ui.gif` 接进工程，建立预处理和播放链路
2. 将 `touch_manager`、`storage_manager`、`main` 统一收敛到 `config/board_config.h`
3. 补 ButtonManager、更真实的 SystemMonitor 电池读取
4. 开始按文档推进 App FSM / Queue 化重构

## 遗留问题

- [ ] 板载喇叭是否已经真实出声仍需现场听感确认；当前只能从串口确认 PCM/I2S/Codec 链路在跑
- [ ] `button_manager` / `gif_player` 仍是占位实现
- [ ] 真实 `ui.gif` 仍未进入运行时显示链路，当前 GIF 区域还是占位动画
- [ ] `system_monitor` 目前电池数据仍为占位值，未接入 AXP2101
- [ ] 触摸控制器当前能响应初始化，但 `FT3168 Device ID` 读回 `0x00`，仍需核实是否为兼容变体或寄存器读取差异
- [ ] 整体控制流仍未完成文档中的 App FSM / Queue 架构

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
