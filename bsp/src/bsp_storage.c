/*
 * bsp_storage.c — SPIFFS + TF 卡（FATFS）
 */
#include <stdio.h>
#include <string.h>
#include "esp_check.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"

#include "bsp_storage.h"
#include "bsp_board_pins.h"

static const char *TAG = "bsp_storage";

static bool s_spiffs_mounted = false;
static bool s_sd_mounted = false;
static sdmmc_card_t *s_sd_card = NULL;
static char s_spiffs_label[17] = BSP_SPIFFS_PARTITION;   /* 记录实际挂载的分区名 */

esp_err_t bsp_spiffs_mount(const char *partition_label)
{
    if (s_spiffs_mounted) {
        return ESP_OK;
    }
    const char *label = (partition_label != NULL) ? partition_label : BSP_SPIFFS_PARTITION;

    const esp_vfs_spiffs_conf_t conf = {
        .base_path = BSP_SPIFFS_MOUNT_POINT,
        .partition_label = label,
        .max_files = 5,
        .format_if_mount_failed = false,
    };
    ESP_RETURN_ON_ERROR(esp_vfs_spiffs_register(&conf), TAG, "SPIFFS register failed");

    size_t total = 0, used = 0;
    esp_err_t ret = esp_spiffs_info(label, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "SPIFFS info failed: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "SPIFFS '%s' mounted at %s (total %u, used %u)",
                 label, BSP_SPIFFS_MOUNT_POINT, (unsigned)total, (unsigned)used);
    }
    s_spiffs_mounted = true;
    strlcpy(s_spiffs_label, label, sizeof(s_spiffs_label));
    return ESP_OK;
}

esp_err_t bsp_spiffs_unmount(void)
{
    if (!s_spiffs_mounted) {
        return ESP_OK;
    }
    esp_err_t ret = esp_vfs_spiffs_unregister(s_spiffs_label);
    s_spiffs_mounted = (ret != ESP_OK);
    return ret;
}

esp_err_t bsp_sdcard_mount(void)
{
    if (s_sd_mounted) {
        return ESP_OK;
    }

    const esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 8 * 1024,
    };

    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_config.width = 1;                      /* 1 线 SD 模式 */
    slot_config.clk = BSP_SD_CLK;
    slot_config.cmd = BSP_SD_CMD;
    slot_config.d0 = BSP_SD_DAT0;
    slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

    ESP_LOGI(TAG, "Mounting SD card (1-bit, CLK=%d CMD=%d D0=%d)", BSP_SD_CLK, BSP_SD_CMD, BSP_SD_DAT0);
    ESP_RETURN_ON_ERROR(esp_vfs_fat_sdmmc_mount(BSP_SD_MOUNT_POINT, &host, &slot_config,
                                                &mount_config, &s_sd_card),
                        TAG, "SD mount failed");
    s_sd_mounted = true;
    sdmmc_card_print_info(stdout, s_sd_card);
    return ESP_OK;
}

esp_err_t bsp_sdcard_unmount(void)
{
    if (!s_sd_mounted) {
        return ESP_OK;
    }
    esp_err_t ret = esp_vfs_fat_sdcard_unmount(BSP_SD_MOUNT_POINT, s_sd_card);
    if (ret == ESP_OK) {
        s_sd_mounted = false;
        s_sd_card = NULL;
    }
    return ret;
}

bool bsp_sdcard_is_mounted(void)
{
    return s_sd_mounted;
}
