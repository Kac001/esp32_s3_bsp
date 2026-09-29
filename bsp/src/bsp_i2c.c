/*
 * bsp_i2c.c — 板载 I2C 总线
 */
#include <stdbool.h>
#include "esp_check.h"
#include "esp_log.h"
#include "driver/gpio.h"

#include "bsp_i2c.h"
#include "bsp_board_pins.h"

static const char *TAG = "bsp_i2c";
static bool s_inited = false;

/* 总线频率由 Kconfig 提供（int 选项，恒有定义） */
#define BSP_I2C_FREQ_HZ  (CONFIG_BSP_I2C_FREQ_KHZ * 1000)

esp_err_t bsp_i2c_init(void)
{
    if (s_inited) {
        return ESP_OK;
    }

    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = BSP_I2C_SDA,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = BSP_I2C_SCL,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = BSP_I2C_FREQ_HZ,
    };
    ESP_RETURN_ON_ERROR(i2c_param_config(BSP_I2C_NUM, &conf), TAG, "i2c_param_config failed");
    ESP_RETURN_ON_ERROR(i2c_driver_install(BSP_I2C_NUM, conf.mode, 0, 0, 0), TAG, "i2c_driver_install failed");

    s_inited = true;
    ESP_LOGI(TAG, "I2C%d ready (SDA=%d SCL=%d %dkHz)", BSP_I2C_NUM, BSP_I2C_SDA, BSP_I2C_SCL,
             conf.master.clk_speed / 1000);
    return ESP_OK;
}

i2c_port_t bsp_i2c_port(void)
{
    return BSP_I2C_NUM;
}
