# dev-prompt.md — 标准开发提示词模板

用下面这一段提示词即可驱动本项目的完整开发流程（AI 会先读
[AGENTS.md](../AGENTS.md) 与 [README.md](README.md)，按约定干活）。

## 模板

```
请基于完整项目【仓库地址，如 https://gitee.com/xxx/esp32-s3】开发【填写应用需求】；
本地已有项目则复用。
先读 AGENTS.md 和 docs/README.md，按需查阅相关文档、硬件接口及示例，
自行准备环境并安装项目要求的 Skill。
保留已有修改，新建分支开发。复用 BSP，重新设计应用界面，不照搬 demo；
中文界面检查字体覆盖。关键需求不明确时询问，其余自主完成。
运行项目验证，交付可从 0x0 刷写的合并固件，说明测试结果与未验证项。
完成后主动询问是否刷机测试，未经确认不烧录。
```

## 成品示例（本仓库，可直接复制使用）

```
请基于完整项目 https://github.com/Kac001/esp32_s3_bsp 开发【填写应用需求】；
本地已有项目则复用。
先读 AGENTS.md 和 docs/README.md，按需查阅相关文档、硬件接口及示例，
自行准备环境并安装项目要求的 Skill。
保留已有修改，新建分支开发。复用 BSP，重新设计应用界面，不照搬 demo；
中文界面检查字体覆盖。关键需求不明确时询问，其余自主完成。
运行项目验证，交付可从 0x0 刷写的合并固件，说明测试结果与未验证项。
完成后主动询问是否刷机测试，未经确认不烧录。
```

> 本项目“项目要求的 Skill”为空：AGENTS.md §2 的环境清单（ESP-IDF、
> Python+Pillow、esptool）就是全部依赖，无需安装额外 Skill。

## 填写说明

| 占位 | 填什么 | 示例 |
|---|---|---|
| 【仓库地址】 | 本项目 Git 地址（本地已有则写本地路径） | `https://gitee.com/me/esp32-s3` |
| 【填写应用需求】 | 一句话说清应用要做什么、面向谁、关键交互 | 见下 |

**应用需求示例**（写清这 3 点就够，其余让 AI 自主决定）：

- 做什么：`做一个 WiFi 时钟 + 天气面板`
- 关键交互：`开机自动联网，触摸切换页面，显示中文城市名与温度`
- 约束（可选）：`界面走深色科技风 / 不用摄像头 / 必须有开机音效`

## 流程保障（提示词背后的约定，已写进 AGENTS.md）

| 提示词要求 | 落地位置 |
|---|---|
| 先读 AGENTS.md 和 docs/README.md | AGENTS.md 为工作约定；docs/ 为文档中心 |
| 自行准备环境 | AGENTS.md §2 环境与工具清单 |
| 安装项目要求的 Skill | AGENTS.md §2：ESP-IDF、Python+Pillow、esptool（字体工具自带，无需 lv_font_conv） |
| 保留已有修改、新建分支 | AGENTS.md §3 Git 纪律 |
| 复用 BSP | AGENTS.md §4.1 + `bsp/` 组件 |
| 重新设计界面、不照搬 demo | AGENTS.md §4.2 + docs/ui-font.md |
| 中文界面检查字体覆盖 | AGENTS.md §4.3 + tools/check_font_coverage.py |
| 关键需求不明时询问 | AGENTS.md §7 |
| 交付 0x0 合并固件 | tools/build_firmware.ps1 + docs/build-flash.md |
| 说明测试结果与未验证项 | AGENTS.md §5.3 交付清单 |
| 完成后询问是否刷机、未确认不烧录 | AGENTS.md §6 刷机纪律 |
