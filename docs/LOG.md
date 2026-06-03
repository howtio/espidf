# 施工日志

> 每次开发 Session 收尾时追加一条。格式：日期 + Phase + 内容 + 提交号。

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
