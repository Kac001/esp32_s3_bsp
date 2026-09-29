/*
 * bsp_touch.c — FT5x06 电容触摸屏
 */
#include "esp_check.h"
#include "esp_log.h"
#include "esp_lcd_touch_ft5x06.h"

#include "bsp_touch.h"
#include "bsp_i2c.h"
#include "bsp_board_pins.h"
#include "bsp_lcd_config_internal.h"

static const char *TAG = "bsp_touch";

static esp_lcd_touch_handle_t s_touch = NULL;   /* 缓存句柄，避免重复创建 */

esp_err_t bsp_touch_new(esp_lcd_touch_handle_t *ret_touch)
{
    ESP_RETURN_ON_FALSE(ret_touch != NULL, ESP_ERR_INVALID_ARG, TAG, "ret_touch is NULL");
    if (s_touch != NULL) {
        *ret_touch = s_touch;
        return ESP_OK;
    }

    const esp_lcd_touch_config_t tp_cfg = {
        .x_max = BSP_TOUCH_X_MAX,
        .y_max = BSP_TOUCH_Y_MAX,
        .rst_gpio_num = GPIO_NUM_NC,
        .int_gpio_num = GPIO_NUM_NC,
        .levels = {
            .reset = 0,
            .interrupt = 0,
        },
        .flags = {
            /* 与 LCD 的 swap_xy / mirror 保持一致 */
            .swap_xy = BSP_LCD_CFG_SWAP_XY,
            .mirror_x = BSP_LCD_CFG_MIRROR_X,
            .mirror_y = BSP_LCD_CFG_MIRROR_Y,
        },
    };

    esp_lcd_panel_io_handle_t tp_io_handle = NULL;
    esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_FT5x06_CONFIG();
    /* FT5x06 的默认宏未填 scl_speed_hz，新版 I2C 驱动会拒绝 0，显式补上 */
    tp_io_config.scl_speed_hz = CONFIG_BSP_I2C_FREQ_KHZ * 1000;
    tp_io_config.transaction_timeout_ms = 1000;

    /* 传入新版 i2c_master 总线句柄，esp_lcd 的 _Generic 会自动选择 v2 实现 */
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_i2c(bsp_i2c_bus(), &tp_io_config, &tp_io_handle),
                        TAG, "New touch panel IO failed");
    ESP_RETURN_ON_ERROR(esp_lcd_touch_new_i2c_ft5x06(tp_io_handle, &tp_cfg, &s_touch),
                        TAG, "New FT5x06 failed");
    *ret_touch = s_touch;
    return ESP_OK;
}
