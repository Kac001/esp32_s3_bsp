/*
 * bsp_io_expander.c — PCA9557 IO 扩展芯片驱动
 */
#include <string.h>
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"

#include "bsp_io_expander.h"
#include "bsp_i2c.h"
#include "bsp_board_pins.h"

static const char *TAG = "bsp_ioe";

#define PCA9557_REG_INPUT   0x00
#define PCA9557_REG_OUTPUT  0x01
#define PCA9557_REG_POLINV  0x02
#define PCA9557_REG_CONFIG  0x03

#define I2C_TIMEOUT_MS      (1000)
#define SET_BITS(_m, _s, _v)  ((_v) ? ((_m) | (_s)) : ((_m) & ~(_s)))

static bool s_inited = false;
static uint8_t s_out_latch = BSP_PCA9557_OUT_DEFAULT;
static i2c_master_dev_handle_t s_dev = NULL;

static esp_err_t pca9557_read(uint8_t reg, uint8_t *data, size_t len)
{
    return i2c_master_transmit_receive(s_dev, &reg, 1, data, len, pdMS_TO_TICKS(I2C_TIMEOUT_MS));
}

static esp_err_t pca9557_write(uint8_t reg, uint8_t value)
{
    uint8_t buf[2] = { reg, value };
    return i2c_master_transmit(s_dev, buf, sizeof(buf), pdMS_TO_TICKS(I2C_TIMEOUT_MS));
}

esp_err_t bsp_ioe_init(void)
{
    if (s_inited) {
        return ESP_OK;
    }

    ESP_RETURN_ON_ERROR(bsp_i2c_init(), TAG, "I2C bus init failed");
    ESP_RETURN_ON_ERROR(bsp_i2c_device_add(BSP_PCA9557_ADDR, &s_dev), TAG, "PCA9557 add device failed");

    /* 上电默认：摄像头休眠、功放关、LCD 不选中 */
    ESP_RETURN_ON_ERROR(pca9557_write(PCA9557_REG_OUTPUT, BSP_PCA9557_OUT_DEFAULT), TAG, "write output failed");
    /* bit0~2 输出，其余保持输入 */
    ESP_RETURN_ON_ERROR(pca9557_write(PCA9557_REG_CONFIG, BSP_PCA9557_CFG_DEFAULT), TAG, "write config failed");

    s_out_latch = BSP_PCA9557_OUT_DEFAULT;
    s_inited = true;
    ESP_LOGI(TAG, "PCA9557 ready (addr 0x%02x)", BSP_PCA9557_ADDR);
    return ESP_OK;
}

esp_err_t bsp_ioe_write(bsp_ioe_pin_t pin, bool level)
{
    ESP_RETURN_ON_FALSE(s_inited, ESP_ERR_INVALID_STATE, TAG, "call bsp_ioe_init() first");

    uint8_t mask = (uint8_t)(1u << pin);
    uint8_t next = (uint8_t)SET_BITS(s_out_latch, mask, level);
    esp_err_t ret = pca9557_write(PCA9557_REG_OUTPUT, next);
    if (ret == ESP_OK) {
        s_out_latch = next;
    }
    return ret;
}

esp_err_t bsp_ioe_read(bsp_ioe_pin_t pin, bool *level)
{
    ESP_RETURN_ON_FALSE(s_inited, ESP_ERR_INVALID_STATE, TAG, "call bsp_ioe_init() first");
    ESP_RETURN_ON_FALSE(level != NULL, ESP_ERR_INVALID_ARG, TAG, "level is NULL");

    uint8_t value = 0;
    ESP_RETURN_ON_ERROR(pca9557_read(PCA9557_REG_OUTPUT, &value, 1), TAG, "read output failed");
    *level = (value >> pin) & 0x01;
    return ESP_OK;
}
