/*
 * bsp_display.h — LCD 显示（ST7789 SPI + LEDC 背光）
 *
 * 说明：LCD_CS 走 PCA9557，本模块在面板初始化时自动拉低片选。
 */
#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "esp_lcd_types.h"
#include "bsp_board_pins.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化背光 PWM（LEDC）。bsp_display_init() 会自动调用。
 */
esp_err_t bsp_display_brightness_init(void);

/**
 * @brief 设置背光亮度
 * @param percent 0~100
 */
esp_err_t bsp_display_brightness_set(int percent);

esp_err_t bsp_display_backlight_on(void);
esp_err_t bsp_display_backlight_off(void);

/**
 * @brief 初始化 LCD（SPI 总线 + ST7789 面板 + 背光）
 *
 * 需要先调用 bsp_board_init()。幂等。
 */
esp_err_t bsp_display_init(void);

/** @brief 获取面板句柄（esp_lcd_panel_draw_bitmap 等直接操作用） */
esp_lcd_panel_handle_t bsp_display_panel(void);

/** @brief 获取面板 IO 句柄（esp_lvgl_port 等需要） */
esp_lcd_panel_io_handle_t bsp_display_io(void);

/**
 * @brief 整屏填充纯色（RGB565）
 */
esp_err_t bsp_display_fill(uint16_t rgb565);

/**
 * @brief 画位图（RGB565，缓冲区须保持到 DMA 完成，建议内部 RAM）
 */
esp_err_t bsp_display_draw_bitmap(int x_start, int y_start, int x_end, int y_end, const void *color_data);

#ifdef __cplusplus
}
#endif
