# 施工日志

> 每次开发 Session 收尾时追加一条。格式：日期 + Phase + 内容 + 提交号。

---

## 2026-06-04 — Phase 3 触摸精确化 + UI 线程安全 + 控件节拍精细调优

- 在上一条触摸节拍化的基础上继续打磨“触摸命中不稳定”的问题
- `display_manager` 引入 FreeRTOS Mutex (`ui_mutex_`) 保护所有 framebuffer 写操作，避免 UI task 与触控回调竞争屏幕缓冲区
- `main.cpp` 触摸逻辑从“按下立即触发”改为“按下-释放在同一控件才触发”：
  - 新增 `pressed_control` 记忆按下时的控件
  - 新增 `touch_release_samples` 消抖计数器，连续 2 次无触摸才判定为释放
  - 只在释放时且 `pressed_control == highlighted` 才执行动作
- 触摸日志更细粒度：
  - 按下打印 `Control down` / `Touch down outside controls`
  - 释放打印 `Control tap`
  - 滑动到不同控件会实时更新高亮
- UI 节拍重新分配：
  - 进度条 250ms（原来 250ms 同时刷进度条+控件，分拆开减少闪烁）
  - 控件按钮 500ms（低频刷新避免与触摸高亮冲突）
  - GIF 占位动画改为 `500ms`，受 `animation_enabled_` 标志控制
- `hit_test_control()` 改为等分三段式热区，不再用像素级精确边界，命中更宽容
- 字体引擎新增 `<` `>` `|` 三个 glyph
- 改动文件：`main/main.cpp`、`components/display_manager/DisplayManager.hpp`、`components/display_manager/DisplayManager.cpp`、`components/display_manager/CMakeLists.txt`、`main/CMakeLists.txt`
- 编译结果：`idf.py build` 通过
- 提交号：`92ee9c2`

---

## 2026-06-04 — Phase 5 预处理 GIF 帧链路接入

- 按施工文档 `Phase 5: GIF 电子宠物` 的稳定路线继续推进，没有在板子上硬塞实时 GIF 解码器
- 当前采用的实现路线与 `tools/setup_sd_card.sh` 一致：
- 资源源文件仍是 `assets/processed/ui_160x160.gif`
- 运行时实际播放数据来自 `/sdcard/pet/frames/frame_000.raw ~ frame_070.raw`
- 这样避免把 3.7MB 原始帧塞进 flash，也避免运行时 GIF 解码和音频抢 CPU
- `GifPlayer` 改动：
- 从纯空壳变成最小可用实现
- `init()` 时申请一块 `160x160 RGB565` 帧缓存（PSRAM）
- 新增 `load_from_directory("/sdcard/pet/frames", 71)`
- `decode_next_frame()` 现在会按序读取预处理 raw 帧
- 支持 `GifLevel` 控制跳帧倍率（FULL/REDUCED/MINIMAL）
- `DisplayManager` 改动：
- 新增 `render_gif_frame_rgb565(...)`
- 将 `160x160 RGB565` 帧转换为 RGB888 后局刷到 GIF 区域
- 继续保持顶部状态栏 / 歌曲区 / 控件区和 GIF 区域解耦
- `main.cpp` 改动：
- SD 卡挂载成功后初始化 `GifPlayer`
- 优先从 `/sdcard/pet/frames` 装载预处理帧
- 主循环每 `250ms` 推进一帧 GIF（当前先用保守节拍，优先稳）
- 占位动画不再作为主路径
- 编译与烧录：
- `idf.py build` 通过
- `esp32-mp3-player.bin` 大小提升到 `0x6a2b0`
- 已重新烧录到 `/dev/ttyACM0`
- 当前明确状态：
- 代码层面已经不是“占位 GIF”，而是真正接上了 `ui_160x160.gif` 的预处理帧播放链路
- 但这一步我还没有补跑一轮 `idf.py monitor` 去确认串口里 `GIF` 周期日志和长期稳定性
- 改动文件：`components/gif_player/GifPlayer.hpp`、`components/gif_player/GifPlayer.cpp`、`components/display_manager/DisplayManager.hpp`、`components/display_manager/DisplayManager.cpp`、`main/main.cpp`
- 提交号：`3263e7a`

---

## 2026-06-04 — Phase 5 GIF 色彩修正 + 粉色主题收敛

- 按用户提供的 `实拍图/` 实拍照片继续收敛显示问题，目标不是继续堆功能，而是先修“颜色不对、UI 不清楚、整机观感不统一”
- 这一步先对比了三组输入：
- `assets/processed/ui_160x160.gif` 的源图
- `/sdcard/pet/frames/frame_000.raw` 的预处理首帧
- 板上实拍图
- 定位结果非常明确：
- 源 GIF 颜色正常
- `raw` 帧读取后直接按本机 `uint16_t RGB565` 解释会偏色
- 用脚本抽查像素发现：
- 同一位置在源 GIF 中是深棕色
- 在 raw565 直接解读后却变成偏绿色
- 将该像素做字节交换后恢复到接近源图颜色
- 结论：`assets/processed/frames/*.raw` 在磁盘上是 big-endian RGB565 字节序，运行时必须先 swap bytes
- `GifPlayer` 改动：
- `decode_next_frame()` 在 `fread()` 后对整帧做一次 `RGB565` 字节交换
- 这样后续显示链路读到的是本机可直接使用的 native `uint16_t`
- `DisplayManager` 改动：
- 整套 UI 色板从橙蓝调改为粉色系，统一成深莓色背景 + 粉色高亮 + 浅粉文本
- GIF 区域底板改成浅粉白色舞台，减少中间动画区发灰发脏的观感
- 标题放大为 `2x`，底部传统播放器图标放大为 `3x`
- 状态栏中部/右侧信息从 dim text 提升到 main text，提高实拍可读性
- 歌曲第二行文本也从 dim 改为 main，减少“看不清标题”的问题
- 实机验证：
- `idf.py build` 通过
- 已重新烧录到 `/dev/ttyACM0`
- 用 PTY 跑了一轮 `idf.py -p /dev/ttyACM0 monitor`
- 串口确认：
- `GIF: [INF] Preprocessed pet frames ready from /sdcard/pet/frames`
- `AUDIO: [INF] Test tone done: 32000 frames, 63 chunks`
- `AUDIO: Streaming frame 100 ... PCM water=49%`
- 这说明本轮显示修正没有打坏 SD/GIF/音频主链路
- 当前仍需继续现场确认的点：
- GIF 色彩虽然已从字节序层面修正，但仍需看你手里的实拍是否已经回到“接近源图”的状态
- 整机屏幕稳定性还需要在真实触摸和长时间播放下继续观察
- 改动文件：`components/gif_player/GifPlayer.cpp`、`components/display_manager/DisplayManager.cpp`
- 提交号：`d711c97`

---

## 2026-06-04 — Phase 5 GIF 切入 Flash + SH8601 对齐修正

- 根据新一轮实拍图继续收敛三个明确问题：
- GIF 角色仍然偏蓝，说明除了 raw 字节序外还存在面板 RGB/BGR 对应问题
- 底部按钮误触明显，原因是之前为了缓解点不到而把热区扩成了大带状
- 歌曲区/标题区出现脏块和轻微乱码，符合 SH8601 对局刷区域需要偶数对齐的已知约束
- 本轮构建链路修正：
- 把 `sdkconfig` / `sdkconfig.defaults` 的 flash size 从 `2MB` 修正为 `16MB`
- 把分区表配置从 `single_app` 切回项目自定义 `partitions.csv`
- `main/CMakeLists.txt` 新增 `spiffs_create_partition_image(assets ../assets/flash FLASH_IN_PROJECT)`
- 新建 `assets/flash/pet/ui_160x160.gif` 和 `assets/flash/pet/frames/*.raw`，只把 GIF 相关资源打包进 flash 的 `assets` 分区
- 实机烧录时已确认 `flash_args` 中出现 `0x410000 build/assets.bin`
- 运行时链路修正：
- `main.cpp` 新增 `mount_assets_partition()`，把 `assets` SPIFFS 挂到 `/assets`
- `GifPlayer` 现在优先从 `/assets/pet/frames` 装载预处理帧，只有失败时才回退到 SD
- 实机串口已确认：
- `ASSET: [INF] Assets SPIFFS mounted: used=3995KB total=7522KB`
- `GIF: [INF] Load preprocessed GIF frames: dir=/assets/pet/frames frames=71`
- `GIF: [INF] Built-in flash GIF frames ready from /assets/pet/frames`
- 显示链路修正：
- `DisplayManager` 把 `panel_cfg.rgb_ele_order` 从 `BGR` 改回 `RGB`
- `present_area()` 现在按 SH8601 约束对局刷区域做偶数边界对齐，减少文字脏块和局部乱码
- 底部命中逻辑从“三等分大热区”改回“真实按钮框 + 小量 padding”
- 顶部文案同步改为 `GIF FROM FLASH`
- 编译与烧录：
- `idf.py build` 通过
- 已重新烧录到 `/dev/ttyACM0`
- 串口确认 A/B 启动、MP3 起播和 `Streaming frame 100 ... PCM water=49%` 仍正常
- 当前明确结论：
- GIF 资源已经进入 flash，不再依赖 SD 上的 `pet/frames`
- 面板色序和 SH8601 对齐问题已经进入当前固件
- 但“颜色观感是否已经完全对齐你的照片预期”仍需要你继续看板确认
- 改动文件：`main/CMakeLists.txt`、`main/main.cpp`、`components/display_manager/DisplayManager.hpp`、`components/display_manager/DisplayManager.cpp`、`sdkconfig`、`sdkconfig.defaults`、`assets/flash/pet/*`
- 提交号：待提交

---

## 2026-06-04 — Phase 3 触摸节拍化 + 传统播放器控件首版

- 按 `CLAUDE.md` / `00_总体施工文档.md` / `04_开发计划.md` 收敛屏幕交互问题，目标是先解决“触摸不工作 / 图标看不见 / 进度条不是歌曲进度”
- 本轮确认：
- 触摸总线不是坏的，根因主要是之前主循环 `5s` 才读一次触摸，天然抓不到正常点击
- 底部控件原来热区过紧，且 5x7 文本图标过小，在 AMOLED 上几乎不可见
- `main.cpp` 侧改动：
- 主循环从单一 `5s heartbeat` 改为多节拍 UI loop
- 触摸轮询节拍改为 `20ms`
- 动画节拍改为 `150ms`
- 进度条节拍改为 `250ms`
- 状态栏/心跳仍保留 `5s`
- 新增触摸控制解析：
- 支持底部 `Prev / PlayPause / Next` 热区命中
- 兼容触摸坐标旋转候选，优先命中可点击控件
- 点中后立即打印控制日志，并触发：
- `PlayPause` → `AudioPlayer::toggle_pause()`
- `Next/Prev` → 设置传输命令并请求当前播放停止切歌
- `audio_player` 侧改动：
- 增加 `track_progress()` 与 `elapsed_seconds()` 基础状态
- 增加 `toggle_pause()`
- `stop()` 改为“请求停止”，避免直接关 I2S 导致播放循环异常
- `play_file()` / `play_test_tone()` 内部现在会检查 `pause/stop`
- 歌曲进度当前先用文件读取位置近似，不再显示 `PCM BUFFER`
- `display_manager` 侧改动：
- 底部按钮改成传统播放器样式 ASCII 图标：
- `|<<`
- `|| / >|`
- `>>|`
- 图标绘制放大为 `2x scale`
- 触摸热区增加 `x/y` padding，解决“点到按钮边缘无响应”
- 进度条文案由 `PCM BUFFER` 改为 `TRACK PROGRESS`
- 继续保留 GIF 区域动画占位，但明确这还不是真正 `ui.gif` 解码播放
- 实机串口验证结果：
- 启动、音频、屏幕链路都稳定
- 首次新增 UI 轮询版本下，已经收到真实触摸日志：
- `TOUCH: [INF] Touch outside controls: raw=(345,201)`
- `TOUCH: [INF] Touch outside controls: raw=(195,429)`
- 这证明：
- 触摸 I2C 和高频轮询已生效
- 当前剩余问题已经不是“触摸无数据”，而是“控件热区和图标可见性需要继续打磨”
- 本轮额外确认 GIF 源文件：
- 施工文档侧真实源资源现确认为 `/home/howtion/biliesp/ui.gif`
- 该文件属性：`GIF89a, 720x720, 1.9MB`
- 当前代码里的中间动画仍是占位动画，不是最终 `ui.gif`
- 结论：
- 触摸链路已从“几乎不可用”推进到“有真实点击日志和控制通路”
- 歌曲进度条已从缓冲条改为曲目进度近似值
- 传统播放器控件已上屏，但真正的 `ui.gif` 解码播放仍是下一步重点
- 改动文件：`main/main.cpp`、`components/audio_player/AudioPlayer.hpp`、`components/audio_player/AudioPlayer.cpp`、`components/display_manager/DisplayManager.hpp`、`components/display_manager/DisplayManager.cpp`
- 编译结果：`idf.py build` 通过；已重新烧录到 `/dev/ttyACM0`
- 提交号：待提交

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
