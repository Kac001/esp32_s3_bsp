# docs/README.md — 文档中心

从这里开始查文档。开发约定（必读）在 [../AGENTS.md](../AGENTS.md)。

## 快速开始（3 步跑起来）

```powershell
# 1) 准备 ESP-IDF v5.1.x 环境
. C:\Espressif\frameworks\esp-idf-v5.1.4\export.ps1   # 路径按本机实际

# 2) 编译并生成 0x0 可烧录的合并固件
powershell -ExecutionPolicy Bypass -File tools\build_firmware.ps1 -Project examples\bsp_smoke

# 3) 烧录（需你确认；未确认不烧录）
powershell -ExecutionPolicy Bypass -File tools\flash_firmware.ps1 -Port COM5 `
    -Firmware examples\bsp_smoke\build\merged-firmware.bin
```

## 文档索引

| 文档 | 内容 | 什么时候看 |
|---|---|---|
| [hardware.md](hardware.md) | 硬件接口：引脚表、外设、总线共享关系、电气注意 | 要用某个外设 / 查引脚 |
| [bsp.md](bsp.md) | BSP API 与用法、初始化顺序、移植到新应用 | 写应用代码前 |
| [ui-font.md](ui-font.md) | 界面设计规范 + 中文字体生成与覆盖检查 | 做界面时 |
| [build-flash.md](build-flash.md) | 构建、合并固件、烧录、串口验证、排错 | 构建/上板时 |
| [examples.md](examples.md) | 01~14 demo 索引：每个 demo 对应的 BSP 模块与参考点 | 想找“某某功能怎么用” |
| [dev-prompt.md](dev-prompt.md) | 标准开发提示词模板与填写说明 | 下达新开发任务时 |

## 仓库地图

```
esp32-s3/
├── AGENTS.md               # ★ 开发约定（AI 开发者必读）
├── bsp/                    # ★ 可复用板级支持包（含 bsp/README.md）
├── examples/bsp_smoke/     # BSP 自检示例（自设计界面）
├── tools/
│   ├── check_font_coverage.py   # 中文字体覆盖检查
│   ├── gen_lvgl_font.py         # LVGL 中文字体子集生成
│   ├── build_firmware.ps1/.sh   # 编译 + 合并固件（0x0）
│   └── flash_firmware.ps1       # 烧录（强制二次确认）
├── docs/                   # 本文档目录
└── 01-boot_key … 14-handheld/  # 厂商 demo（只作参考）
```

## 常见问题

- **界面出现“豆腐块”（方框）**：字体没覆盖该字符，跑
  `python tools/check_font_coverage.py --font <字体.c> --src <源码>`，见 [ui-font.md](ui-font.md)。
- **编译找不到组件**：确认工程顶层 CMakeLists.txt 设置了
  `EXTRA_COMPONENT_DIRS` 指向 `bsp/`（照抄 `examples/bsp_smoke/CMakeLists.txt`）。
- **屏幕花屏/方向不对**：Kconfig 里调整 `BSP_LCD_*`（旋转/镜像/反色/SPI 频率）。
- **合并固件烧录后不启动**：确认烧录地址是 `0x0`（合并镜像），不要拿
  `app.bin` 单独烧 `0x10000` 之外的地址。
