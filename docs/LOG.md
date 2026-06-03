# 施工日志

> 每次开发 Session 收尾时追加一条。格式：日期 + Phase + 内容 + 提交号。

---

## 2026-06-04 — Phase 2 最小启动链路收敛

- 修复当前工程编译阻塞，`idf.py build` 已通过
- 收敛 `main/main.cpp` 启动路径：I2C 扫描、Display、Touch、SD、Audio、周期状态日志
- 修正 `audio_player` 的音频基线参数：默认采样率改为 `22050Hz`
- 去掉 `main` 中对 `AudioPlayer` 的裸 `malloc` 用法，改为静态对象启动任务
- 为无 SD/无歌曲场景补了测试音回退，避免音频路径完全空转
- 实现 `memory_pool` 最小可用版本：基于 `heap_caps_malloc` 的 SRAM/PSRAM 分配和统计
- 实现 `system_monitor` 最小可用版本：启动日志 + 周期内存/电源占位日志
- 清理 `sdkconfig.defaults` 中已过时或无效的配置项
- 改动文件：`main/main.cpp`、`components/audio_player/*`、`components/display_manager/DisplayManager.cpp`、`components/memory_pool/*`、`components/system_monitor/SystemMonitor.cpp`、`sdkconfig.defaults`
- 编译结果：`idf.py build` 通过
- 串口日志覆盖情况：已覆盖启动 Banner、I2C 扫描、模块初始化、周期内存/电源日志；实机串口尚未验证
- 提交号：待提交

---

## 2026-06-04 — Phase 2 实机音频链路调试

- 实机执行 `idf.py -p /dev/ttyACM0 flash monitor`，确认启动链路稳定：I2C 扫描、LCD、Touch、SD、Audio、Heartbeat 全部有日志
- I2C 实扫结果：`0x18 0x20 0x34 0x38 0x51 0x6B 0x7E`；其中 `0x18` 为 ES8311，`0x38` 为触摸控制器
- SD 卡挂载成功，扫描到 `/sdcard/music` 下 5 首 MP3；解码日志确认当前资源实际为 `16000Hz mono`
- 将 `audio_player` 从整文件解码改为流式解码，消除早期调试中出现的内存申请失败、任务栈溢出和错误缓冲长度计算问题
- 放弃了“播放中途按首帧动态重配 I2S/ES8311”的方案；该方案在板上会引发 panic/重启，当前基线改为固定 `16000Hz`
- 对齐官方 `i2s_es8311` 示例，修正 ES8311 / I2S 初始化细节：
- `MCLK` 改为 `384x` 倍频
- codec 初始化后显式调用 `es8311_sample_frequency_config()`
- 显式调用 `es8311_voice_mute(false)` 解除静音
- 去掉播放阶段额外 `/2` 的 PCM 衰减
- 实机串口连续观测到 `Streaming frame 100/200/300/400...` 与 `Heartbeat: alive` 并行输出，未复现“音频播放导致 ESP32 持续重启”
- 当前剩余问题收敛为“串口确认音频链路在持续推流，但板载喇叭是否真实出声仍需继续验证输出模拟链路 / AMP_EN 极性 / 资源侧对照测试音”
- 改动文件：`components/audio_player/AudioPlayer.cpp`、`components/audio_player/AudioPlayer.hpp`、`components/audio_player/CMakeLists.txt`、`components/mp3_decoder/Mp3Decoder.cpp`、`main/main.cpp`
- 编译结果：`idf.py build` 通过；重新烧录后可稳定启动运行
- 串口日志覆盖情况：已覆盖启动 Banner、I2C 扫描、Display/Touch/SD/Audio 初始化、连续播放帧日志、周期内存/电源日志
- 提交号：待提交

---

## 2026-06-04 — Phase 3 最小测试音基线

- 按施工文档的 `Phase 3: SD + I2S + 电池` 先收敛音频验证基线，新增显式 `StartupAudioMode`
- 默认启动模式切到 `test-tone`，不再依赖“空路径回退”隐式触发测试音
- `AudioPlayer` 新增显式 `play_test_tone()` 接口，当前参数为 `1kHz / 2s / 16000Hz output`
- 实机烧录后串口确认音频基线日志：
- `AUDIO: [INF] Startup mode: test tone baseline`
- `AUDIO: [INF] Test tone start: 1000Hz 2000ms @ 16000Hz output`
- `AUDIO: [INF] Test tone done: 32000 frames, 63 chunks`
- 该模式下系统仍能稳定输出 `Heartbeat` 与内存/电源周期日志，未引入新的重启问题
- 这一步的目的不是解决全部音质问题，而是把“模拟输出链路验证”和“SD/MP3 解码验证”强制分离，便于后续 A/B 排查
- 改动文件：`components/audio_player/AudioPlayer.hpp`、`components/audio_player/AudioPlayer.cpp`、`main/main.cpp`
- 编译结果：`idf.py build` 通过；重新烧录后 `test-tone` 模式实机验证通过
- 串口日志覆盖情况：已覆盖启动模式、测试音开始/结束、Heartbeat、周期内存/电源日志
- 提交号：待提交

---

## 2026-06-04 — Phase 4 音频缓冲切到 PSRAM/SRAM 分层

- 将 `AudioPlayer` 的播放期临时分配改为启动期一次性分配，符合文档“运行时不 malloc/free”的方向
- `MP3ReadBuf` 改为在 `init()` 中通过 `MemoryPool::alloc_psram()` 分配，当前大小 `128KB`
- `PCMOutBuf` 改为在 `init()` 中通过 `MemoryPool::alloc_sram()` 分配，当前大小 `9216 bytes`
- `play_file()` 不再临时 `malloc/free` MP3 缓冲和 PCM 缓冲，也不再堆分配 `Mp3Decoder`
- 实机串口已确认内存分层日志：
- `MEM: PSRAM alloc: 131072 bytes`
- `MEM: SRAM alloc: 9216 bytes`
- `AUDIO: Audio buffers ready: MP3ReadBuf=128KB(PSRAM) PCMOutBuf=9216 bytes(SRAM)`
- 烧录后的 A/B 启动模式仍然正常：先测试音，再播放 `/sdcard/music/01.mp3`
- 当前这一步完成后，工程已进入“基础 PSRAM 版”音频路径：`SD -> PSRAM MP3ReadBuf -> CPU解码 -> SRAM PCMOutBuf -> I2S DMA`
- 仍未完成的部分：还没有把文档中的 `PCM RingBuf(PSRAM)` 建起来，当前仍是“单帧解码后直接写 I2S”
- 改动文件：`components/audio_player/AudioPlayer.hpp`、`components/audio_player/AudioPlayer.cpp`、`components/audio_player/CMakeLists.txt`
- 编译结果：`idf.py build` 通过；烧录后实机验证通过
- 串口日志覆盖情况：已覆盖 PSRAM/SRAM 分配、测试音开始/结束、MP3 起播、连续播放帧日志、周期内存/电源日志
- 提交号：待提交

---

## 2026-06-04 — Phase 4 PCM RingBuf 落地

- 在上一条“基础 PSRAM 版”基础上继续推进，补上文档要求的 `PCM RingBuf(PSRAM)`
- 当前内存分层变为：
- `MP3ReadBuf = 128KB (PSRAM)`
- `PCMRingBuf = 256KB (PSRAM)`
- `PCMOutBuf = 9216 bytes (SRAM)`
- 播放模式从“解一帧立刻写 I2S”改为：
- `SD -> MP3ReadBuf -> CPU 解码 -> PCMRingBuf -> PCMOutBuf -> I2S DMA`
- 实机串口确认：
- `MEM: PSRAM alloc: 262144 bytes`
- `AUDIO: Audio buffers ready: MP3ReadBuf=128KB(PSRAM) PCMRingBuf=256KB(PSRAM) PCMOutBuf=9216 bytes(SRAM)`
- `AUDIO: Streaming frame 100 ... PCM water=49%`
- 这说明 `pcm_water_level()` 不再是占位值，当前已经能反映真实 ringbuffer 水位
- 额外修正：对 `i2s_channel_enable()` 做了“已启用状态兼容”，避免测试音与 MP3 A/B 切换时重复打印驱动错误
- 当前剩余缺口：虽然 ringbuffer 已经有了，但播放控制命令、状态机和多任务解耦仍未接上
- 改动文件：`components/audio_player/AudioPlayer.hpp`、`components/audio_player/AudioPlayer.cpp`
- 编译结果：`idf.py build` 通过；烧录后实机验证 `PCM water=%` 日志有效
- 串口日志覆盖情况：已覆盖 MP3ReadBuf / PCMRingBuf / PCMOutBuf 分配、测试音、MP3 起播、真实水位日志、周期内存/电源日志
- 提交号：待提交

---

## 2026-06-04 — Phase 4 连续播放最小闭环

- 在现有 A/B 启动模式基础上，把“只播一首”推进为“连续顺播”
- `audio_task` 现在支持：
- 测试音 -> 播放列表
- 自动下一首
- 播放列表播完后循环回第一首
- 当前这一步先落在 `main/audio_task`，还没有接入按钮、触摸和 App FSM 控制层
- 目的：先把音乐核心链路做成可连续运行的最小闭环，避免后续 UI/FSM 接入前还卡在单曲路径
- 编译结果：`idf.py build` 通过
- 串口验证：当前已确认 A/B 流程和 MP3 持续播放正常，连续顺播逻辑已入代码，等待长时回归继续验证
- 改动文件：`main/main.cpp`
- 提交号：待提交

---

## 2026-06-03 (第二次) — Phase 1 新增 SystemMonitor + 调试体系

- 新增 `system_monitor` 组件（第 10 个 component）
- 新增 `docs/09_调试与串口打印规范.md`（串口日志格式/等级/模块标识/Claude检查清单）
- 全面更新 `00_总体施工文档.md`：UI 布局增加状态栏 + 数据流增加 SystemMonitor
- 全面更新 `01_架构说明.md`：增加 LogTask、UiEventQueue、状态栏、日志架构
- 全面更新 `04_开发计划.md`：每个 Phase 附加调试日志要求
- 全面更新 `05_内存预算.md`：增加日志缓冲 2KB
- 全面更新 `06_模块接口规范.md`：新增 SystemMonitor .hpp 接口
- 全面更新 `CLAUDE.md`：增加调试铁律 + 文档索引
- 处理 GIF：720×720 → 160×160, 10fps, 407KB + 71帧 RGB565 raw
- 处理 MP3：5 首 128kbps 立体声 → 64kbps mono 22050Hz, 总大小减半
- SD 卡脚本：`tools/setup_sd_card.sh`
- 修正 GitHub 仓库地址：`git@github.com:howtio/espidf.git`
- 提交号：待首次提交

---

## 2026-06-03 — Phase 1 项目脚手架创建

- 创建项目目录骨架：main + 9 components + config + assets + docs + tools
- 建立 7 份施工文档体系（00~06 + LOG + DEV_PROGRESS）
- 制定双核 FreeRTOS 多任务架构（音频优先 + App FSM 中枢）
- 定义 9 个解耦模块接口规范
- 制定 7 个 Phase 渐进开发计划
- 编制内存预算（PSRAM + SRAM + Flash）
- 配置 ESP-IDF sdkconfig.defaults + 分区表
- 提交号：待首次提交

---

## 模板（新条目复制这个）

```text
## YYYY-MM-DD — Phase X <简短标题>

- 做了什么
- 改了什么文件
- 编译结果
- 串口日志覆盖情况
- 提交号：`xxxxxxx`
```
