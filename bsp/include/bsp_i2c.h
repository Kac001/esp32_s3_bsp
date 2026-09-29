/*
 * bsp_i2c.h — 板载 I2C 总线（触摸 / IO 扩展 / IMU / 摄像头 SCCB / 音频 Codec 共用）
 */
#pragma once

#include "esp_err.h"
#include "driver/i2c.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化 I2C 主机（幂等）
 */
esp_err_t bsp_i2c_init(void);

/** @brief 获取 I2C 外设号（供驱动组件如 esp_camera 使用） */
i2c_port_t bsp_i2c_port(void);

#ifdef __cplusplus
}
#endif
