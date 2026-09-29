# examples.md — demo 索引（01~14）

厂商 demo 是**参考资料**，不是依赖，且**不随仓库分发**（.gitignore 已排除；
如需参考，另行获取后放到仓库根目录的同名文件夹即可）。找“某功能怎么用”时按本表定位，
然后用 [bsp.md](bsp.md) 的方式在 BSP 上实现，**不要复制 demo 代码**。

| demo | 内容 | 对应 BSP 模块 | 参考点（看什么） |
|---|---|---|---|
| 01-boot_key | GPIO0 按键中断 | （应用层） | 按键中断/队列写法；引脚宏 `BSP_BUTTON_BOOT` |
| 02-attitude | QMI8658 倾角 | `bsp_imu_*` | 注意 demo 版读不到 ID 会死循环，BSP 已修 |
| 03-micro_sd | TF 卡 FATFS | `bsp_sdcard_*` | 挂载/读写流程 |
| 04-audio_es7210 | ES7210 录音 | `bsp_audio_read/feed_data` | 旧版 es7210.h 驱动，已被 esp_codec_dev 取代 |
| 05-audio_es8311 | ES8311 播放 | `bsp_audio_write` | 旧版 es8311.h + 嵌入 PCM 的播放方式 |
| 06-lcd | LCD 画图 | `bsp_display_*` | 裸屏绘制、色块、图片数组 |
| 07-lcd_camera | 摄像头上屏 | `bsp_camera_*` + `bsp_display_draw_bitmap` | 双任务取流/刷屏；GC0308 镜像设置 |
| 08-lcd_lvgl | LVGL 控件 | `bsp_lvgl_init` | lvgl_port 用法（lvgl_port_lock 等） |
| 09-wifi_scan_connect | WiFi 扫描+连接+中文 UI | （应用层）+ 中文字体 | 中文字符串 + font_alipuhui20 用法 |
| 10-ble_hid_device | BLE HID 键盘 | （应用层） | NimBLE HID profile |
| 11-mp3_player | MP3 播放器 | `bsp_audio_*` + `bsp_spiffs_mount` | esp-audio-player 组件用法；注意其采样率宏与实际 I2S 时钟不符（BSP 已修） |
| 12-speech_recognition | 语音识别（esp-sr） | `bsp_audio_feed_data` | wakenet 唤醒 + multinet 中文命令词；模型放 model 分区 |
| 13-human_face_detection | 人脸检测（esp-dl） | `bsp_camera_*` | esp-dl 组件 + C++ 互操作（extern "C"） |
| 14-handheld | 综合手持终端 | 全部 | 六宫格 UI、开机音乐、多任务；**UI 只可参考不可照搬** |

## 按需求查表

| 我要做 | 先看 |
|---|---|
| 界面/菜单 | 08（控件）、14（多页面，仅参考交互）、[ui-font.md](ui-font.md) |
| 中文显示 | 09、14 的 `font_alipuhui20.c` + [ui-font.md](ui-font.md) |
| 音乐播放 | 11、14（esp-audio-player）→ `bsp_audio_*` |
| 录音/语音 | 04、12 → `bsp_audio_feed_data` |
| 拍照/图像 | 07、13 → `bsp_camera_*` |
| 文件/TF 卡 | 03、11 → `bsp_storage_*` |
| 传感器 | 02、14 → `bsp_imu_*` |
| WiFi/BLE | 09、10（应用层，BSP 不含无线） |
| 开机音乐 | 14 的 `power_music_task` 思路（BSP 只提供 `bsp_audio_write`） |

## demo 已知问题（提取 BSP 时已处理，看 demo 时留意）

1. `esp32_s3_szp.*` 在各 demo 复制 11 份且有差异（参数/声明不一致）；
2. 部分头文件“声明无实现”（`bsp_speaker_set_fs`、`lcd_draw_pictrue` 等）；
3. 11 的 `CODEC_DEFAULT_SAMPLE_RATE=48000` 与硬编码 16kHz I2S 时钟矛盾；
4. 02 版 QMI8658 初始化读不到 ID 会死循环；
5. 09 的摄像头代码块缺 include，打开开关即编译失败；
6. 04/05 用旧版 codec 驱动（es7210.h/es8311.h），与 11/12/14 的
   esp_codec_dev 不是一套。
