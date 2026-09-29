/*
 * bsp_i2c.h — 板载 I2C 总线（触摸 / IO 扩展 / IMU / 摄像头 SCCB / 音频 Codec 共用）
 *
 * 注意：使用 ESP-IDF 5.x 新版 i2c_master 驱动（driver/i2c_master.h）。
 * IDF 5.5 下 esp32-camera / esp_codec_dev 均已使用新驱动，新旧驱动不能共存，
 * 因此 BSP 统一采用新驱动；摄像头 SCCB 通过 i2c_master_get_bus_handle(0) 复用本总线。
 */
#pragma once

#include "esp_err.h"
#include "driver/i2c_master.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化 I2C 主机总线（幂等）
 */
esp_err_t bsp_i2c_init(void);

/** @brief 获取 I2C 总线句柄（esp_lcd v2 / esp_codec_dev / 通用传输用） */
i2c_master_bus_handle_t bsp_i2c_bus(void);

/**
 * @brief 在总线上添加一个 I2C 器件（7 位地址），返回器件句柄
 * @param addr_7bit 7 位器件地址
 * @param out_dev   器件句柄
 */
esp_err_t bsp_i2c_device_add(uint16_t addr_7bit, i2c_master_dev_handle_t *out_dev);

#ifdef __cplusplus
}
#endif
