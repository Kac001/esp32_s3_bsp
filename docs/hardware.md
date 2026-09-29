# hardware.md — 硬件接口

板卡：**立创开发板 · 实战派（ESP32-S3）**。唯一权威引脚定义：
[../bsp/include/bsp_board_pins.h](../bsp/include/bsp_board_pins.h)（本文档是它的说明视图）。

## 芯片与存储

| 项目 | 规格 |
|---|---|
| 主控 | ESP32-S3（双核 Xtensa，最高 240MHz） |
| Flash | 16MB（`CONFIG_ESPTOOLPY_FLASHSIZE_16MB`） |
| PSRAM | 8MB OPI（八线，80MHz） |
| 显示 | ST7789 320x240 SPI，16bpp RGB565 |
| 触摸 | FT5x06（I2C 电容触摸） |
| 摄像头 | DVP 接口（GC0308 系列） |
| 音频 | ES8311（DAC/功放）+ ES7210（4 路麦克风 ADC） |
| 传感器 | QMI8658 六轴（加速度计 + 陀螺仪） |
| 存储 | TF 卡（SDMMC 1 线）+ SPIFFS 分区 |
| IO 扩展 | PCA9557（I2C 0x19） |

## 引脚总表

### I2C0（多器件共用总线）

| 引脚 | 功能 |
|---|---|
| GPIO1 | SDA |
| GPIO2 | SCL |

总线速率 100kHz（Kconfig `BSP_I2C_FREQ_KHZ` 可调）。挂载器件：

| 器件 | 地址 | 用途 |
|---|---|---|
| PCA9557 | 0x19 | IO 扩展（LCD_CS / PA_EN / DVP_PWDN） |
| FT5x06 | 0x38（模组默认） | 触摸屏 |
| QMI8658 | 0x6A | 六轴姿态 |
| ES8311 | 0x18（组件默认） | 音频 DAC |
| ES7210 | 0x41（8 位写地址 0x82） | 麦克风 ADC |
| 摄像头 SCCB | — | 复用 I2C0（`pin_sccb_sda=-1`） |

### PCA9557 IO 扩展（bit 位）

| bit | 信号 | 极性/说明 | 上电默认 |
|---|---|---|---|
| 0 | LCD_CS | 低有效 | 1（不选中） |
| 1 | PA_EN | 高有效（功放） | 0（关） |
| 2 | DVP_PWDN | 1=摄像头休眠，0=工作 | 1（休眠） |

### LCD（SPI3_HOST）

| 引脚 | 功能 |
|---|---|
| GPIO40 | MOSI |
| GPIO41 | CLK |
| GPIO39 | DC |
| GPIO42 | 背光（LEDC0，反相，默认 Kconfig 打开） |
| — | CS = PCA9557 bit0（无 GPIO） |
| — | RST = 无 |

SPI 80MHz（可降频），spi_mode=2，8 位命令/参数。旋转 swap_xy=1、mirror_x=1
（横屏安装，Kconfig 可改）。

### 触摸（FT5x06，I2C）

| 引脚 | 功能 |
|---|---|
| — | 复用 I2C0；无 INT/RST 线（轮询） |

坐标变换与 LCD 旋转联动（swap_xy/mirror 同源配置）。

### 摄像头（DVP，8 位）

| 引脚 | 功能 | | 引脚 | 功能 |
|---|---|---|---|---|
| GPIO5 | XCLK（24MHz） | | GPIO16 | D0 |
| GPIO1 | SIOD（I2C0 SDA） | | GPIO18 | D1 |
| GPIO2 | SIOC（I2C0 SCL） | | GPIO8 | D2 |
| GPIO3 | VSYNC | | GPIO17 | D3 |
| GPIO46 | HREF | | GPIO15 | D4 |
| GPIO7 | PCLK | | GPIO6 | D5 |
| — | PWDN = PCA9557 bit2 | | GPIO4 | D6 |
| — | RESET = 无 | | GPIO9 | D7 |

### 音频（I2S_NUM_1 + Codec）

| 引脚 | 功能 |
|---|---|
| GPIO38 | MCLK |
| GPIO14 | BCLK |
| GPIO13 | WS |
| GPIO45 | DOUT（喇叭） |
| GPIO12 | DIN（麦克风） |
| — | 功放使能 = PCA9557 bit1 |

ES7210 4 路麦克风（MIC1~4 全开）；esp-sr 使用 `bsp_audio_feed_data()` 的
4→3 通道重排（MIC1/MIC3/参考）。

### TF 卡（SDMMC_HOST_SLOT1，1 线）

| 引脚 | 功能 |
|---|---|
| GPIO47 | CLK |
| GPIO48 | CMD |
| GPIO21 | DAT0 |

### 按键

| 引脚 | 功能 |
|---|---|
| GPIO0 | BOOT 键（低有效；逻辑由应用实现，BSP 只给引脚宏） |

## 分区表（默认，16MB）

```
nvs      data  nvs     0x9000   24k
phy_init data  phy     0xf000   4k
factory  app   factory          8M
storage  data  spiffs           3M   ← SPIFFS（/spiffs）
```

烧录偏移：bootloader `0x0`、分区表 `0x8000`、boot_app0 `0xe000`、
app `0x10000`、storage 按分区表自动计算（合并脚本自动处理）。

## 电气/使用注意

1. **I2C 总线共享**：触摸/IO 扩展/IMU/Codec/摄像头 SCCB 都在 I2C0，
   应用不要重复初始化 I2C（用 `bsp_i2c_init()`）；
2. **LCD_CS 必须先拉低**（BSP 已处理），否则屏幕无响应；
3. **摄像头与 LCD 共用 PCA9557**：摄像头供电前需 I2C 就绪；
4. **DMA 与 PSRAM 互斥**：LVGL 显示缓冲用内部 DMA RAM（BSP 默认）；
   大图缓冲放 PSRAM 时注意传输时序；
5. **音频采样率**：改采样率用 `bsp_audio_set_fs()`（同时重配 I2S 时钟），
   不要只改 Codec。
