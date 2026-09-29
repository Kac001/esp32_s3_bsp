/*
 * bsp_i2c.c — 板载 I2C 总线（新版 i2c_master 驱动）
 */
#include <stdbool.h>
#include "esp_check.h"
#include "esp_log.h"

#include "bsp_i2c.h"
#include "bsp_board_pins.h"

static const char *TAG = "bsp_i2c";
static i2c_master_bus_handle_t s_bus = NULL;

/* 总线频率由 Kconfig 提供（int 选项，恒有定义） */
#define BSP_I2C_FREQ_HZ  (CONFIG_BSP_I2C_FREQ_KHZ * 1000)

esp_err_t bsp_i2c_init(void)
{
    if (s_bus != NULL) {
        return ESP_OK;
    }

    const i2c_master_bus_config_t bus_cfg = {
        .i2c_port = BSP_I2C_NUM,
        .sda_io_num = BSP_I2C_SDA,
        .scl_io_num = BSP_I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&bus_cfg, &s_bus), TAG, "i2c_new_master_bus failed");

    ESP_LOGI(TAG, "I2C%d ready (SDA=%d SCL=%d %dkHz)", BSP_I2C_NUM, BSP_I2C_SDA, BSP_I2C_SCL,
             CONFIG_BSP_I2C_FREQ_KHZ);
    return ESP_OK;
}

i2c_master_bus_handle_t bsp_i2c_bus(void)
{
    return s_bus;
}

esp_err_t bsp_i2c_device_add(uint16_t addr_7bit, i2c_master_dev_handle_t *out_dev)
{
    ESP_RETURN_ON_FALSE(s_bus != NULL, ESP_ERR_INVALID_STATE, TAG, "call bsp_i2c_init() first");
    ESP_RETURN_ON_FALSE(out_dev != NULL, ESP_ERR_INVALID_ARG, TAG, "out_dev is NULL");

    const i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = addr_7bit,
        .scl_speed_hz = BSP_I2C_FREQ_HZ,
    };
    return i2c_master_bus_add_device(s_bus, &dev_cfg, out_dev);
}
