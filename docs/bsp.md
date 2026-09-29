# bsp.md — BSP 使用指南

API 参考见 [../bsp/README.md](../bsp/README.md)；本文档是“怎么用”的菜谱。
引脚与硬件背景见 [hardware.md](hardware.md)。

## 0. 接入工程

```cmake
# 工程顶层 CMakeLists.txt
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../../bsp")   # 按实际路径
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(my_app)
```

托管组件（lvgl/esp_lvgl_port/ft5x06/esp_codec_dev/esp32-camera）由
`bsp/idf_component.yml` 自动拉取，应用无需重复声明。

## 1. 最小可用（点亮屏幕）

```c
#include "bsp_board.h"
#include "bsp_display.h"

void app_main(void)
{
    bsp_board_init();                 // I2C + PCA9557
    bsp_display_init();               // SPI + ST7789 + 背光
    bsp_display_fill(0xF800);         // 整屏红色（RGB565）
}
```

## 2. LVGL 界面（应用自行设计）

```c
#include "bsp_lvgl.h"

lv_disp_t *disp = bsp_lvgl_init();    // 显示 + 触摸 + LVGL 环境

if (lvgl_port_lock(portMAX_DELAY)) {
    lv_obj_t *lab = lv_label_create(lv_scr_act());
    lv_label_set_text(lab, "你好");
    lv_obj_center(lab);
    lvgl_port_unlock();
}
```

要点：
- 所有 LVGL 对象操作都包在 `lvgl_port_lock/unlock` 中；
- 中文文本先过字体覆盖检查（见 [ui-font.md](ui-font.md)）；
- 界面布局/配色/文案按需求重新设计，不要照搬 demo。

## 3. 音频

```c
#include "bsp_audio.h"

bsp_audio_init();                                   // I2S + ES8311 + ES7210
bsp_audio_set_volume(70);
size_t written;
bsp_audio_write(pcm_buf, pcm_len, &written, portMAX_DELAY);   // 播放（自动开功放）
bsp_audio_mute(true);

// 录音（esp-sr 前处理格式：MIC1/MIC3/参考 3 通道）
int16_t feed[1024];
bsp_audio_feed_data(false, feed, sizeof(feed));
```

换采样率一律走 `bsp_audio_set_fs(rate, bits, ch)`（会重配 I2S 时钟）。

## 4. 摄像头

```c
#include "bsp_camera.h"

bsp_camera_init(NULL);                // 默认 QVGA/RGB565/2 帧
camera_fb_t *fb = esp_camera_fb_get();
if (fb) {
    bsp_display_draw_bitmap(0, 0, fb->width, fb->height, fb->buf);
    esp_camera_fb_return(fb);
}
```

## 5. 存储

```c
#include "bsp_storage.h"

bsp_spiffs_mount(NULL);               // "storage" 分区 -> /spiffs
bsp_sdcard_mount();                   // TF 卡 -> /sdcard（无卡返回错误）
FILE *f = fopen("/sdcard/music.mp3", "rb");
```

## 6. IMU

```c
#include "bsp_imu.h"

bsp_imu_init();
bsp_imu_data_t d;
if (bsp_imu_read(&d) == ESP_OK) {
    printf("angle: %.1f %.1f %.1f\n", d.angle_x, d.angle_y, d.angle_z);
}
uint8_t motion = bsp_imu_motion_status();
```

## 7. 组合初始化顺序

`bsp_board_init()` 必须最先；其余模块按需、任意顺序（内部自动处理依赖）。
摄像头与音频都依赖 I2C/PCA9557，放在 `bsp_board_init()` 之后即可。

## 8. 需要新能力时

1. 先查 [examples.md](examples.md) 里对应 demo 的做法；
2. 能力属于“板级”的 → 加进 `bsp/`（新头文件 + 实现 + Kconfig 开关），
   并更新 `bsp/README.md`；
3. 能力属于“应用”的 → 放应用里；
4. 任何情况下**不要在应用里写引脚号**。
