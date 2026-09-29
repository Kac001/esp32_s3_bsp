# bsp — 立创实战派 ESP32-S3 板级支持包

从本仓库 14 个 demo 的 `esp32_s3_szp.*` 板级代码中提炼而成的**独立可复用组件**。
只负责“把板子点亮并交给应用”：显示、触摸、摄像头、音频、存储、IMU、LVGL 环境。
**不含任何界面与业务逻辑**（界面请自行设计，不要照搬 demo）。

## 目录结构

```
bsp/
├── CMakeLists.txt          # 按 Kconfig 裁剪编译哪些模块
├── Kconfig                 # 时钟/旋转/模块开关（menuconfig → “BSP” 菜单）
├── idf_component.yml       # 托管组件依赖（lvgl/esp_lvgl_port/ft5x06/esp_codec_dev/esp32-camera）
├── include/
│   ├── bsp_board_pins.h    # ★ 唯一的引脚映射（换板只改这里）
│   ├── bsp_board.h         # 板级总入口
│   ├── bsp_i2c.h           # I2C 总线（触摸/IO扩展/IMU/Codec 共用）
│   ├── bsp_io_expander.h   # PCA9557（LCD_CS / PA_EN / DVP_PWDN）
│   ├── bsp_display.h       # ST7789 + 背光
│   ├── bsp_touch.h         # FT5x06 触摸
│   ├── bsp_lvgl.h          # LVGL 移植层（esp_lvgl_port）
│   ├── bsp_camera.h        # DVP 摄像头
│   ├── bsp_audio.h         # ES8311 放音 + ES7210 4 路麦克风
│   ├── bsp_storage.h       # SPIFFS + TF 卡
│   └── bsp_imu.h           # QMI8658 六轴
└── src/                    # 实现（含内部头 bsp_lcd_config_internal.h）
```

## 在应用工程中使用

方式一（推荐）：把 `bsp/` 复制到工程的 `components/bsp/`；或在顶层 CMakeLists.txt 中：

```cmake
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../../bsp")   # 按实际相对路径
```

然后在应用中：

```c
#include "bsp_board.h"
#include "bsp_display.h"
#include "bsp_lvgl.h"

void app_main(void)
{
    bsp_board_init();                 // I2C + PCA9557（安全默认态）
    lv_disp_t *disp = bsp_lvgl_init(); // LCD + 触摸 + LVGL

    // …… 界面由应用自行设计 ……
}
```

## 典型初始化顺序

| 序号 | 调用 | 作用 | 依赖 |
|---|---|---|---|
| 1 | `bsp_board_init()` | I2C + IO 扩展（LCD_CS=1、功放关、摄像头休眠） | — |
| 2 | `bsp_lvgl_init()` 或 `bsp_display_init()` | 显示（+触摸+LVGL） | 1 |
| 3 | `bsp_audio_init()` | I2S + ES8311 + ES7210 | 1 |
| 4 | `bsp_camera_init(NULL)` | 摄像头上电并初始化 | 1 |
| 5 | `bsp_spiffs_mount(NULL)` / `bsp_sdcard_mount()` | 文件系统 | — |
| 6 | `bsp_imu_init()` | QMI8658 | 1 |

各模块幂等，可按需组合；不调用即不占资源。被 Kconfig 关闭的模块不参与编译
（对应 API 不可用，误调用会链接失败）。

## API 速查

| 模块 | 主要接口 |
|---|---|
| 板级 | `bsp_board_init` `bsp_board_name` |
| I2C | `bsp_i2c_init` `bsp_i2c_bus` `bsp_i2c_device_add` |
| IO 扩展 | `bsp_ioe_init` `bsp_ioe_write/read` |
| 显示 | `bsp_display_init` `bsp_display_fill` `bsp_display_draw_bitmap` `bsp_display_brightness_set` `bsp_display_panel` |
| 触摸 | `bsp_touch_new` |
| LVGL | `bsp_lvgl_init` `bsp_lvgl_touch_indev` |
| 摄像头 | `bsp_camera_init` `bsp_camera_power` `bsp_camera_deinit` |
| 音频 | `bsp_audio_init` `bsp_audio_set_fs` `bsp_audio_write/read` `bsp_audio_feed_data` `bsp_audio_set_volume` `bsp_audio_mute` `bsp_audio_pa_enable` |
| 存储 | `bsp_spiffs_mount/unmount` `bsp_sdcard_mount/unmount` `bsp_sdcard_is_mounted` |
| IMU | `bsp_imu_init` `bsp_imu_read` `bsp_imu_motion_status` `bsp_imu_close` |

挂载点：SPIFFS=`/spiffs`（分区名 `storage`），TF 卡=`/sdcard`。

## 相对 demo 的修正（提取时处理的坑）

1. **I2S 时钟不再硬编码 16kHz**：`bsp_audio_set_fs()` 同时重配 I2S 时钟与 Codec（demo 11 的 48000 宏与实际时钟不符）。
2. **QMI8658 不再死等**：ID 重试 3 次失败返回 `ESP_ERR_NOT_FOUND`（02 版会挂死）。
3. **清理“声明无实现”**：demo 中 `bsp_speaker_set_fs`、`lcd_draw_pictrue`、部分 `qmi8658_*` 声明了却没有实现，本组件 API 全部有实现。
4. **LCD_CS 由 PCA9557 控制**：面板初始化前自动拉低片选；功放 PA_EN、摄像头 DVP_PWDN 同理由组件内部管理。
5. **旋转一致性**：面板 swap/mirror、LVGL rotation、触摸坐标变换三处由同一组 Kconfig 值驱动，杜绝各写各的。
6. **统一音频栈**：不再使用 04/05 demo 的旧版 `es7210.h`/`es8311.h` 驱动，统一 esp_codec_dev（ES7210 4 路麦克风全使能）。
7. **行缓冲常驻**：`bsp_display_fill()` 使用常驻 DMA 行缓冲，避免 demo 中“释放早于 DMA 完成”的隐患。
8. **I2C 迁移到新驱动**（driver/i2c_master.h）：IDF 5.5 下 esp32-camera / esp_codec_dev
   均使用新 I2C 驱动，新旧驱动不能共存（启动即 abort）；BSP 统一新驱动，
   摄像头 SCCB 按端口号复用同一总线，触摸屏 IO 走 esp_lcd v2 实现。

## Kconfig 说明

`idf.py menuconfig → BSP — 板级支持包`：

- `BSP_I2C_FREQ_KHZ`（默认 100）
- `BSP_LCD_SPI_FREQ_MHZ`（默认 80）、`BSP_LCD_SWAP_XY / MIRROR_X / MIRROR_Y / INVERT_COLOR`
- `BSP_LCD_BACKLIGHT_INVERTED`（默认开）
- 模块开关：`BSP_ENABLE_TOUCH / LVGL / CAMERA / AUDIO / IMU / STORAGE`

## 换板/换屏

只改 `include/bsp_board_pins.h`（引脚与器件地址）+ Kconfig（旋转、极性）。
若外设组合变化（如无摄像头），再配合模块开关裁剪。
