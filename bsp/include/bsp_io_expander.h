/*
 * bsp_io_expander.h — PCA9557 IO 扩展芯片（LCD_CS / PA_EN / DVP_PWDN）
 */
#pragma once

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BSP_IOE_LCD_CS = 0,     /* LCD 片选，低有效 */
    BSP_IOE_PA_EN = 1,      /* 音频功放使能，高有效 */
    BSP_IOE_CAM_PWDN = 2,   /* 摄像头电源，1=休眠 0=工作 */
} bsp_ioe_pin_t;

/** @brief 初始化 PCA9557（须先 bsp_i2c_init）。幂等。 */
esp_err_t bsp_ioe_init(void);

/** @brief 设置 IO 扩展引脚电平 */
esp_err_t bsp_ioe_write(bsp_ioe_pin_t pin, bool level);

/** @brief 读取 IO 扩展引脚电平（读输出锁存） */
esp_err_t bsp_ioe_read(bsp_ioe_pin_t pin, bool *level);

#ifdef __cplusplus
}
#endif
