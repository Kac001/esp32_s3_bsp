# build-flash.md — 构建、合并固件与烧录验证

## 环境准备

1. 安装 **ESP-IDF v5.1.x 或 v5.5.x**（BSP 已在 5.5.5 实测）：
   <https://docs.espressif.com/projects/esp-idf/zh_CN/v5.5.5/esp32s3/get-started/>
2. 每个终端先初始化环境：
   - Windows：`. C:\Espressif\frameworks\esp-idf-v5.5.5\export.ps1`
   - Linux/macOS：`. $HOME/esp/esp-idf/export.sh`
   - 若装在非默认路径（如 `D:\Espressif`）：先 `$env:IDF_TOOLS_PATH='D:\Espressif'`；
   - 若提示“禁止运行脚本”：先 `Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass -Force`；
3. 确认：`idf.py --version`、`python -m esptool version`。
4. 字体工具用 Python 3.8+（需 Pillow）；本机 bundled Python 已含。

## 编译

```powershell
cd <工程目录>                # 如 examples\bsp_smoke
idf.py set-target esp32s3    # 首次
idf.py build
```

新应用建议直接复制 `examples/bsp_smoke/` 骨架（sdkconfig.defaults + partitions.csv）。

## 合并固件（交付物）

```powershell
powershell -ExecutionPolicy Bypass -File tools\build_firmware.ps1 -Project <工程目录>
```

脚本做的事：`idf.py build` → 读取 `build/flasher_args.json` 的全部映像与偏移
→ `esptool merge_bin` 生成 **`build/merged-firmware.bin`（0x0 起整片镜像）**。

为什么交付合并固件：单文件、单地址（0x0），任何烧录器/量产夹具都能一键写入，
不依赖分区表细节。默认分区下的组成：

| 偏移 | 内容 |
|---|---|
| 0x0 | bootloader |
| 0x8000 | 分区表 |
| 0xe000 | boot_app0 |
| 0x10000 | 应用（factory） |
| 0x810000 | storage（SPIFFS，如有） |

> 偏移以 `build/flash_args.txt`（脚本同时生成）为准——换分区表后自动变化。

Linux/CI 用 `tools/build_firmware.sh <工程目录>`。

## 烧录（须用户确认！）

```powershell
powershell -ExecutionPolicy Bypass -File tools\flash_firmware.ps1 -Port COM5 `
    -Firmware <工程目录>\build\merged-firmware.bin
```

脚本会显示摘要并要求输入 `yes` 二次确认。**未经用户确认禁止烧录**（AGENTS.md
第 6 条）。分段烧录备用命令：

```powershell
python -m esptool --chip esp32s3 -p COM5 -b 460800 write_flash 0x0 <工程目录>\build\merged-firmware.bin
```

## 上板验证

1. 接 USB 串口，确认 COM 号（设备管理器 / `python -m esptool chip_id`）；
2. 看日志：`idf.py -p COM5 monitor`（115200）；
3. 验证清单（按应用裁剪）：
   - [ ] 启动日志正常、无 panic/复位循环；
   - [ ] 屏幕显示 UI，方向/颜色正确；
   - [ ] 触摸响应；
   - [ ] 扬声器出声、麦克风录音正常；
   - [ ] TF 卡挂载、中文文件名正常（GBK 配置）；
   - [ ] 摄像头出图（如使用）；
   - [ ] IMU 数据变化（如使用）；
   - [ ] 中文无“豆腐块”。
4. 交付时写明：**测试结果**与**未验证项**（没测的写清原因）。

## 排错

| 现象 | 处理 |
|---|---|
| 编译报找不到 `bsp` 组件 | 顶层 CMakeLists.txt 的 `EXTRA_COMPONENT_DIRS` 路径不对 |
| 托管组件下载失败 | 无网络/代理问题；配置 `IDF_COMPONENT_MANAGER` 代理或离线拷贝 managed_components |
| 屏幕白屏/花屏 | 降 SPI 频率 `BSP_LCD_SPI_FREQ_MHZ=40`；检查旋转/反色 Kconfig |
| 方向/触摸错位 | `BSP_LCD_SWAP_XY/MIRROR_*` 与触摸会同步，改一处即可 |
| 烧录握手失败 | 按住 BOOT 再复位进入下载模式；换线/降波特率 `-b 115200` |
| 烧录后不启动 | 确认烧的是 `merged-firmware.bin` 且地址 0x0；查看串口日志 |
| 中文乱码（SD 文件名） | sdkconfig 需 `FATFS_CODEPAGE_936` + `FATFS_API_ENCODING_UTF_8` |
| 界面缺字方框 | 跑字体覆盖检查，见 [ui-font.md](ui-font.md) |
