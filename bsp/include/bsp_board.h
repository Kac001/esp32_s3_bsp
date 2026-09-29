/*
 * bsp_board.h — 板级支持包（BSP）总入口
 *
 * 设计约定：
 *   1. BSP 只负责“把板子点亮”，不包含任何应用界面/业务逻辑；
 *   2. 各子模块（显示、触摸、摄像头、音频、存储、IMU…）可独立初始化，
 *      bsp_board_init() 仅做无副作用的公共底座（I2C + IO 扩展芯片）；
 *   3. 所有对外符号使用 bsp_ / BSP_ 前缀。
 */
#pragma once

#include "esp_err.h"

#include "bsp_board_pins.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 板级公共底座初始化：I2C 总线 + PCA9557 IO 扩展
 *
 * 安全默认状态：LCD_CS=1（不选中）、PA_EN=0（功放关）、DVP_PWDN=1（摄像头休眠）。
 * 重复调用是安全的（幂等）。
 *
 * @return ESP_OK 成功
 */
esp_err_t bsp_board_init(void);

/** @brief 板卡名称，如 "lckfb-esp32s3-shizhanpai" */
const char *bsp_board_name(void);

/** @brief 板卡版本号 */
const char *bsp_board_revision(void);

#ifdef __cplusplus
}
#endif
