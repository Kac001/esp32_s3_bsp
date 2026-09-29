/*
 * bsp_board.c — 板级公共底座（I2C + IO 扩展）
 */
#include "esp_check.h"
#include "esp_log.h"

#include "bsp_board.h"
#include "bsp_i2c.h"
#include "bsp_io_expander.h"

static const char *TAG = "bsp_board";

esp_err_t bsp_board_init(void)
{
    esp_err_t ret = bsp_i2c_init();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "I2C init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = bsp_ioe_init();
    ESP_RETURN_ON_ERROR(ret, TAG, "PCA9557 init failed");

    ESP_LOGI(TAG, "Board '%s' rev %s init OK", BSP_BOARD_NAME, BSP_BOARD_REVISION);
    return ESP_OK;
}

const char *bsp_board_name(void)
{
    return BSP_BOARD_NAME;
}

const char *bsp_board_revision(void)
{
    return BSP_BOARD_REVISION;
}
