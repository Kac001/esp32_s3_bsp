/*
 * bsp_imu.h — QMI8658 六轴姿态传感器（I2C）
 */
#pragma once

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 六轴原始数据 + 由加速度计解算的倾角 */
typedef struct {
    int16_t acc_x, acc_y, acc_z;
    int16_t gyr_x, gyr_y, gyr_z;
    float   angle_x, angle_y, angle_z;   /* 单位：度 */
} bsp_imu_data_t;

/**
 * @brief 初始化 QMI8658（含运动检测配置）。须先 bsp_i2c_init()。
 */
esp_err_t bsp_imu_init(void);

/** @brief 让传感器进入休眠 */
void bsp_imu_close(void);

/**
 * @brief 读取加速度/陀螺仪并解算倾角
 */
esp_err_t bsp_imu_read(bsp_imu_data_t *data);

/**
 * @brief 读取运动状态寄存器（STATUS1）：
 *        bit0 AnyMotion / bit1 NoMotion / bit2 SigMotion（详见数据手册）
 */
uint8_t bsp_imu_motion_status(void);

#ifdef __cplusplus
}
#endif
