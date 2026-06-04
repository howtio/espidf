# 开发进度

> 当前进度、下一步计划、遗留问题。每次收尾更新。

---

## 当前版本：0.1.0

## 当前 Phase：Phase 5 — GIF 实装后的显示收敛

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
- [x] `GifPlayer` 已接入 `/sdcard/pet/frames/*.raw` 预处理帧链路，主屏 GIF 区域已具备真实资源播放路径
- [x] 已按最新实拍图继续做 `UI` 信息减法：去掉顶部标题、压缩状态栏、去掉 `TRACK PROGRESS` 小字、歌曲区只保留更少的信息
- [x] 底部控制按钮已放大到 `90x52`，几何图标同步放大，便于实拍和手指操作
- [x] 进度条与按钮区位置已重新整理，底部边缘拥挤感进一步下降
- [x] GIF 运行时缓存策略已从整包预载改成 `5` 帧 PSRAM 窗口缓存，避免启动期大块预载和 watchdog 风险
- [x] GIF 运行时路径已稳定为 `Flash assets SPIFFS -> PSRAM 5帧窗口 -> framebuffer -> LCD`
- [x] UI 已去掉重复静态文案，当前只保留单一标题，不再叠 `PET UI / GIF FROM FLASH / PINK UI ...`
- [x] 歌曲区、进度区、控制区已改成整块清屏后重绘，并增加 `1.5s` 低频整屏复刷以压掉残影
- [x] 已完成 `build + flash + monitor` 回归，串口确认 `GIF window primed: 5 frames`，音频播放和 `Heartbeat` 仍正常
- [x] 已确认 `assets/processed/frames/*.raw` 需要运行时做 `RGB565` 字节交换，GIF 偏色根因已定位并修正到 `GifPlayer`
- [x] 主 UI 已整体切换为粉色系主题，并提升了标题、状态栏和底部控件的对比度与字号
- [x] 已结合 `实拍图/` 做一轮显示观感修正，并完成 `build + flash + monitor` 回归
- [x] 已修正 `16MB flash + 自定义 partitions.csv` 构建配置，`assets` SPIFFS 分区重新生效
- [x] GIF 预处理帧已切入 flash：当前运行时优先从 `/assets/pet/frames` 装载，不再依赖 SD 上的 `pet/frames`
- [x] SH8601 局刷已加入偶数边界对齐，底部按钮热区已从大带状收回到真实按钮框

## 下一步
1. 继续对照 `实拍图/` 收顶栏、歌曲区和按钮区的清晰度；如果仍脏，下一步考虑改更大的像素字或直接做位图字体
2. 将触摸命令从 UI 主循环直控改成入队消费，减少连按时的卡死风险
3. 将 `touch_manager`、`storage_manager`、`main` 统一收敛到 `config/board_config.h`
4. 补 ButtonManager、更真实的 SystemMonitor 电池读取

## 遗留问题

- [ ] 板载喇叭是否已经真实出声仍需现场听感确认；当前只能从串口确认 PCM/I2S/Codec 链路在跑
- [ ] `button_manager` 仍是占位实现
- [ ] 真实 `ui.gif` 已接入 flash 预处理帧链路，但仍需继续实机长时验证刷新稳定性和颜色观感
- [ ] UI 观感虽已去掉双层文案并增加整屏复刷，但仍需继续根据最新实拍图收细节脏块和文字清晰度
- [ ] `system_monitor` 目前电池数据仍为占位值，未接入 AXP2101
- [ ] 触摸控制器当前能响应初始化，但 `FT3168 Device ID` 读回 `0x00`，仍需核实是否为兼容变体或寄存器读取差异
- [ ] 触摸控制当前仍是 UI 线程直控音频，快速连按时稳定性还不够
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
