# 立创实战派 ESP32-S3 开发工程

ESP32-S3（立创开发板·实战派）应用开发仓库：**可复用 BSP + 文档 + 工具链**，
配套厂商 demo 集（01~14）作为参考资料。

## 从哪里开始

- 开发约定（AI 开发者必读）：[AGENTS.md](AGENTS.md)
- 文档中心：[docs/README.md](docs/README.md)
- 标准开发提示词模板：[docs/dev-prompt.md](docs/dev-prompt.md)

## 目录

| 路径 | 说明 |
|---|---|
| [bsp/](bsp/) | 板级支持包：显示/触摸/摄像头/音频/存储/IMU/LVGL（含 [bsp/README.md](bsp/README.md)） |
| [examples/bsp_smoke/](examples/bsp_smoke/) | BSP 自检示例（自设计界面） |
| [tools/](tools/) | 中文字体覆盖检查、LVGL 字体生成、构建/合并/烧录脚本 |
| [docs/](docs/) | 硬件接口、BSP 用法、UI 字体、构建烧录、demo 索引 |
| `01-boot_key/` … `14-handheld/` | 厂商 demo（只作参考，不作依赖） |

## 60 秒上手

```powershell
. <ESP-IDF>\export.ps1                                     # 准备环境
powershell -File tools\build_firmware.ps1 -Project examples\bsp_smoke
powershell -File tools\flash_firmware.ps1 -Port COM5 `
    -Firmware examples\bsp_smoke\build\merged-firmware.bin   # 烧录需你确认
```

新应用开发请直接使用 [docs/dev-prompt.md](docs/dev-prompt.md) 的标准提示词。
