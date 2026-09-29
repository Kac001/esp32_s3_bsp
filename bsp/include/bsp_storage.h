/*
 * bsp_storage.h — 存储（SPIFFS 分区 + TF 卡 FATFS）
 */
#pragma once

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BSP_SPIFFS_MOUNT_POINT   "/spiffs"
#define BSP_SPIFFS_PARTITION     "storage"
#define BSP_SD_MOUNT_POINT       "/sdcard"

/**
 * @brief 挂载 SPIFFS（分区名默认 "storage"，可传 NULL）
 */
esp_err_t bsp_spiffs_mount(const char *partition_label);

/** @brief 卸载 SPIFFS */
esp_err_t bsp_spiffs_unmount(void);

/**
 * @brief 挂载 TF 卡（SDMMC 1 线，FATFS，挂载点 /sdcard）
 */
esp_err_t bsp_sdcard_mount(void);

/** @brief 卸载 TF 卡 */
esp_err_t bsp_sdcard_unmount(void);

/** @brief TF 卡是否已挂载 */
bool bsp_sdcard_is_mounted(void);

#ifdef __cplusplus
}
#endif
