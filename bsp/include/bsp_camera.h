/*
 * bsp_camera.h — DVP 摄像头（esp32-camera 组件）
 *
 * 电源控制：DVP_PWDN 走 PCA9557，本模块自动上电。
 * SCCB 与板载 I2C0 共用总线。
 */
#pragma once

#include <stdbool.h>
#include "esp_err.h"
#include "esp_camera.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 摄像头配置（NULL 项使用默认值） */
typedef struct {
    pixformat_t pixel_format;   /* 默认 PIXFORMAT_RGB565 */
    framesize_t frame_size;     /* 默认 FRAMESIZE_QVGA */
    int fb_count;               /* 帧缓冲数量，默认 2 */
    bool hmirror;               /* 水平镜像（GC0308 默认开） */
} bsp_camera_config_t;

/**
 * @brief 摄像头上电（退出休眠）
 */
esp_err_t bsp_camera_power(bool on);

/**
 * @brief 初始化摄像头。cfg 为 NULL 时使用默认配置（QVGA/RGB565/2 帧）。
 *
 * 需要先调用 bsp_board_init()。
 */
esp_err_t bsp_camera_init(const bsp_camera_config_t *cfg);

/**
 * @brief 反初始化摄像头并使其休眠
 */
esp_err_t bsp_camera_deinit(void);

#ifdef __cplusplus
}
#endif
