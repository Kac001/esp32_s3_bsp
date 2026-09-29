# AGENTS.md — 开发工作约定（AI 开发者必读）

> 本文件是“标准开发提示词”的落地约定。接到开发任务后：**先读本文件与
> [docs/README.md](docs/README.md)**，再按需查阅硬件接口与示例文档，然后动手。

---

## 1. 项目是什么

- 硬件：**立创开发板 · 实战派（ESP32-S3）**，16MB Flash + 8MB OPI PSRAM，
  ST7789 320x240 SPI 屏（触摸 FT5x06）、DVP 摄像头、ES8311+ES7210 音频、
  QMI8658 六轴、TF 卡、PCA9557 IO 扩展。完整引脚见
  [docs/hardware.md](docs/hardware.md)。
- 本仓库结构：
  - `bsp/` — **可复用板级支持包**（显示/触摸/摄像头/音频/存储/IMU/LVGL 环境）；
  - `examples/bsp_smoke/` — BSP 自检示例（自设计界面，仅作参考）；
  - `tools/` — 字体工具链 + 构建/烧录脚本；
  - `docs/` — 文档（从这里开始查）；
  - 厂商 demo（01~14、hello_world）**不随仓库分发**（.gitignore 已排除）；
    如需参考，另行获取后放同名目录即可，[docs/examples.md](docs/examples.md)
    有“demo → BSP 模块”对照表。

## 2. 开始前：环境与工具

**Skill 要求：本项目不依赖任何额外 Skill**——下表即全部环境要求，装好即可开工。

| 要求 | 说明 |
|---|---|
| ESP-IDF | v5.1.x / v5.5.x（BSP 已在 5.1.4（demo）与 5.5.5（实测）验证）；构建前 `export.ps1` / `export.sh`。若装在非默认路径（如 `D:\Espressif`），先设 `$env:IDF_TOOLS_PATH`；脚本被执行策略拦截时加 `Set-ExecutionPolicy -Scope Process Bypass` |
| Python | 3.8+（字体工具用；需 Pillow，本机 bundled Python 已含） |
| esptool | 随 IDF 提供（`python -m esptool`） |
| 串口驱动 | 烧录/看日志需要（CH340/CP210x 按板载芯片） |
| 可选 | lv_font_conv（Node）；本仓库 `tools/gen_lvgl_font.py` 可替代 |

环境不全时先自行安装/修复；仍不可用则在回复中明确列出“未验证项”。

## 3. Git 纪律

1. **保留已有修改**：不 stash、不 revert、不覆盖未提交内容；
2. **新建分支开发**：`git switch -c feat/<需求简述>`，不在主分支直接改；
3. 提交信息用中文或英文祈使句，一个功能点一个提交。

## 4. 开发规则

1. **复用 BSP**：板级外设一律走 `bsp/` API（`bsp_board_init`、`bsp_display_*`、
   `bsp_audio_*`、`bsp_camera_*`、`bsp_storage_*`、`bsp_imu_*`、`bsp_lvgl_init`）。
   - 禁止把 `esp32_s3_szp.*` 或 demo 的 `main/` 代码复制进应用；
   - 需要新外设能力时，优先扩展 BSP（改 `bsp/` 并更新 `bsp/README.md`），
     不要在应用里散写引脚号；
   - 引脚只存在于 `bsp/include/bsp_board_pins.h`。
2. **界面重新设计**：demo 的 UI（14-handheld 的六宫格菜单等）**只可参考交互，
   不可照搬**。界面布局/配色/文案按本次需求重新设计。
3. **中文界面必须检查字体覆盖**（否则会出现“豆腐块”）：
   ```bash
   # 提取用字 → 生成/选择字体 → 校验覆盖
   # 注意：--src 要包含所有提供 UI 字符串的目录（含 bsp/ 等组件），
   #       并用 --extra-ascii 兜底数字/英文（漏扫就会出现"lckfb-esp▢▢"式方框）
   python tools/check_font_coverage.py --src <应用源码目录> --src bsp/include \
        --emit-symbols symbols.txt
   python tools/gen_lvgl_font.py --chars-file symbols.txt --extra-ascii --size 20 \
        --name font_ui_cn_20 -o <应用目录>/font_ui_cn_20.c
   python tools/check_font_coverage.py --font <应用目录>/font_ui_cn_20.c \
        --src <应用源码目录> --src bsp/include
   ```
   最后一条命令必须输出“全部字符均被字体覆盖 ✓”（exit 0），详见
   [docs/ui-font.md](docs/ui-font.md)。
4. 中文文案统一 UTF-8；SD 卡文件名中文需 FATFS GBK 配置（模板已含）。

## 5. 构建、验证与交付

1. 编译：`idf.py set-target esp32s3 && idf.py build`（示例工程含
   `sdkconfig.defaults` 与分区表，新应用照抄）；
2. 合并固件（**交付物必须包含**）：
   ```powershell
   powershell -ExecutionPolicy Bypass -File tools\build_firmware.ps1 -Project <工程目录>
   ```
   产物 `build/merged-firmware.bin` 为 **0x0 起整片镜像**，可一键烧录；
3. 交付回复必须包含：
   - 合并固件路径；
   - **测试结果**：做了哪些验证（编译/静态检查/字体覆盖/上板现象）；
   - **未验证项**：哪些没测、为什么（如本机无 ESP-IDF、无硬件在手）；
4. 上板验证时：接好串口，`idf.py monitor` 或串口工具 115200 看日志。

## 6. 刷机纪律（重要）

- **未经用户明确确认，禁止烧录**（包括“顺手试一下”）；
- 交付完成后**主动询问**：“是否现在烧录测试？”，等用户确认后再执行；
- 烧录只用 `tools/flash_firmware.ps1`（自带二次确认）或等价的
  `python -m esptool write_flash 0x0 build/merged-firmware.bin`。

## 7. 沟通

- **关键需求不明确时先问**（界面风格、交互、硬件外设取舍、引脚冲突等）；
  其余细节自主决定，不要用问题轰炸用户；
- 遇到与本文件冲突的用户显式要求，以用户为准，并在交付说明中标注差异。

## 8. 标准任务提示词（模板）

新开任务可直接使用（替换【】内容）：

> 请基于完整项目【仓库地址】开发【填写应用需求】；本地已有项目则复用。
> 先读 AGENTS.md 和 docs/README.md，按需查阅相关文档、硬件接口及示例，
> 自行准备环境并安装项目要求的 Skill。
> 保留已有修改，新建分支开发。复用 BSP，重新设计应用界面，不照搬 demo；
> 中文界面检查字体覆盖。关键需求不明确时询问，其余自主完成。
> 运行项目验证，交付可从 0x0 刷写的合并固件，说明测试结果与未验证项。
> 完成后主动询问是否刷机测试，未经确认不烧录。
