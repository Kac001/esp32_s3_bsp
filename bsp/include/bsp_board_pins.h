/*
 * bsp_board_pins.h — 板级引脚映射（唯一的硬件连线定义处）
 *
 * 板卡：立创开发板 · 实战派（ESP32-S3，lckfb）
 *   - LCD：ST7789 320x240 SPI 屏，CS 由 PCA9557 IO 扩展芯片控制
 *   - 触摸：FT5x06 I2C 电容触摸
 *   - 摄像头：DVP（GC0308 系列），电源开关 DVP_PWDN 由 PCA9557 控制
 *   - 音频：ES8311（放音）+ ES7210（4 路麦克风），功放使能 PA_EN 由 PCA9557 控制
 *   - 存储：TF 卡（SDMMC 1 线）+ SPIFFS（storage 分区）
 *   - 姿态：QMI8658（I2C）
 *
 * 复用到其它板卡时，只需要改动本文件（以及 Kconfig 里的少量选项），
 * 各驱动模块的实现无需变化。
 */
#pragma once

#include "esp_bit_defs.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/i2s_std.h"

/******************************************************************************/
/* 版本 / 板名 ****************************************************************/
#define BSP_BOARD_NAME            "lckfb-esp32s3-shizhanpai"
#define BSP_BOARD_REVISION        "1.0"

/* BOOT 键（GPIO0，低有效）——仅做引脚映射，按键逻辑由应用实现 */
#define BSP_BUTTON_BOOT           (GPIO_NUM_0)

/******************************************************************************/
/* I2C 总线（共享：触摸 / IO 扩展 / 姿态 / 摄像头 SCCB / 音频 Codec） **********/
#define BSP_I2C_SDA               (GPIO_NUM_1)
#define BSP_I2C_SCL               (GPIO_NUM_2)
#define BSP_I2C_NUM               (0)                 /* I2C 外设号 */
/* 总线频率见 Kconfig（BSP_I2C_FREQ_KHZ，默认 100kHz） */

/******************************************************************************/
/* PCA9557 IO 扩展芯片（I2C 地址 0x19） ***************************************/
#define BSP_PCA9557_ADDR          (0x19)
/* 输出寄存器位定义 */
#define BSP_PCA9557_LCD_CS        BIT(0)              /* LCD 片选，低有效 */
#define BSP_PCA9557_PA_EN         BIT(1)              /* 功放使能，高有效 */
#define BSP_PCA9557_DVP_PWDN      BIT(2)              /* 摄像头电源，1=休眠 0=工作 */
/* 上电默认值：DVP_PWDN=1（摄像头休眠）、PA_EN=0（功放关）、LCD_CS=1（不选中） */
#define BSP_PCA9557_OUT_DEFAULT   (0x05)
#define BSP_PCA9557_CFG_DEFAULT   (0xf8)              /* bit0~2 输出，其余输入 */

/******************************************************************************/
/* LCD 显示屏（ST7789，SPI） **************************************************/
#define BSP_LCD_SPI_NUM           (SPI3_HOST)
#define BSP_LCD_SPI_MOSI          (GPIO_NUM_40)
#define BSP_LCD_SPI_CLK           (GPIO_NUM_41)
#define BSP_LCD_SPI_CS            (GPIO_NUM_NC)       /* CS 走 PCA9557，无 GPIO */
#define BSP_LCD_DC                (GPIO_NUM_39)
#define BSP_LCD_RST               (GPIO_NUM_NC)       /* 无复位线 */
#define BSP_LCD_BACKLIGHT         (GPIO_NUM_42)       /* LEDC PWM 背光 */
/* SPI 时钟见 Kconfig（BSP_LCD_SPI_FREQ_MHZ，默认 80MHz） */
#define BSP_LCD_CMD_BITS          (8)
#define BSP_LCD_PARAM_BITS        (8)
#define BSP_LCD_SPI_MODE          (2)
#define BSP_LCD_BITS_PER_PIXEL    (16)
#define BSP_LCD_H_RES             (320)               /* 旋转后横向 320 */
#define BSP_LCD_V_RES             (240)               /* 旋转后纵向 240 */
#define BSP_LCD_DRAW_BUF_HEIGHT   (20)                /* LVGL 每缓存行数 */
#define BSP_LCD_LEDC_TIMER        (LEDC_TIMER_0)
#define BSP_LCD_LEDC_CHANNEL      (LEDC_CHANNEL_0)
#define BSP_LCD_BACKLIGHT_FREQ_HZ (5000)
/* 背光极性、旋转/镜像见 Kconfig（BSP_LCD_BACKLIGHT_INVERTED / BSP_LCD_SWAP_XY …） */

/******************************************************************************/
/* 触摸屏（FT5x06，I2C） ******************************************************/
#define BSP_TOUCH_X_MAX           (BSP_LCD_V_RES)     /* swap_xy 后触摸宽 = 屏高 */
#define BSP_TOUCH_Y_MAX           (BSP_LCD_H_RES)     /* swap_xy 后触摸高 = 屏宽 */

/******************************************************************************/
/* 摄像头（DVP） **************************************************************/
#define BSP_CAM_XCLK              (GPIO_NUM_5)
#define BSP_CAM_SIOD              (BSP_I2C_SDA)       /* 与 I2C0 共用 */
#define BSP_CAM_SIOC              (BSP_I2C_SCL)
#define BSP_CAM_D7                (GPIO_NUM_9)
#define BSP_CAM_D6                (GPIO_NUM_4)
#define BSP_CAM_D5                (GPIO_NUM_6)
#define BSP_CAM_D4                (GPIO_NUM_15)
#define BSP_CAM_D3                (GPIO_NUM_17)
#define BSP_CAM_D2                (GPIO_NUM_8)
#define BSP_CAM_D1                (GPIO_NUM_18)
#define BSP_CAM_D0                (GPIO_NUM_16)
#define BSP_CAM_VSYNC             (GPIO_NUM_3)
#define BSP_CAM_HREF              (GPIO_NUM_46)
#define BSP_CAM_PCLK              (GPIO_NUM_7)
#define BSP_CAM_PWDN              (-1)                /* 电源走 PCA9557 */
#define BSP_CAM_RESET             (-1)
#define BSP_CAM_XCLK_FREQ_HZ      (24 * 1000 * 1000)

/******************************************************************************/
/* 音频（I2S + ES8311 + ES7210） **********************************************/
#define BSP_I2S_NUM               (I2S_NUM_1)
#define BSP_I2S_MCLK              (GPIO_NUM_38)
#define BSP_I2S_BCLK              (GPIO_NUM_14)
#define BSP_I2S_WS                (GPIO_NUM_13)
#define BSP_I2S_DOUT              (GPIO_NUM_45)       /* 喇叭数据 */
#define BSP_I2S_DIN               (GPIO_NUM_12)       /* 麦克风数据 */
#define BSP_AUDIO_PA_PIN          (-1)                /* 功放走 PCA9557，无 GPIO */

/* ES8311（DAC）使用组件默认地址；ES7210（ADC）8 位写地址 0x82（7 位 0x41） */
#define BSP_ES7210_ADDR_8BIT      (0x82)
#define BSP_AUDIO_MIC_CHANNELS    (4)                 /* ES7210 4 路麦克风 */

/******************************************************************************/
/* TF 卡（SDMMC 1 线模式） ****************************************************/
#define BSP_SD_CMD                (GPIO_NUM_48)
#define BSP_SD_CLK                (GPIO_NUM_47)
#define BSP_SD_DAT0               (GPIO_NUM_21)

/******************************************************************************/
/* 姿态传感器 QMI8658（I2C） **************************************************/
#define BSP_QMI8658_ADDR          (0x6A)
