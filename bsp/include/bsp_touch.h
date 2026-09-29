/*
 * bsp_touch.h — FT5x06 电容触摸屏
 */
#pragma once

#include "esp_err.h"
#include "esp_lcd_touch.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 创建触摸屏句柄（须先 bsp_i2c_init()）
 * @param[out] ret_touch 触摸句柄
 */
esp_err_t bsp_touch_new(esp_lcd_touch_handle_t *ret_touch);

#ifdef __cplusplus
}
#endif
