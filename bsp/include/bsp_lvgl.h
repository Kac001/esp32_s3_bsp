/*
 * bsp_lvgl.h — LVGL 移植层（基于 esp_lvgl_port）
 *
 * 应用界面请自行设计；BSP 只提供“屏幕 + 触摸 + LVGL 线程环境”。
 */
#pragma once

#include "esp_err.h"
#include "lvgl.h"
#include "esp_lvgl_port.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化 LVGL 并注册显示屏（与触摸输入设备）
 *
 * 内部依次完成：lvgl_port_init() → bsp_display_init() → bsp_touch_new()。
 * 返回后应用可用 lvgl_port_lock()/lvgl_port_unlock() 保护 UI 操作。
 *
 * @return 显示设备句柄；失败返回 NULL
 */
lv_disp_t *bsp_lvgl_init(void);

/** @brief 获取触摸输入设备句柄（未启用触摸时返回 NULL） */
lv_indev_t *bsp_lvgl_touch_indev(void);

#ifdef __cplusplus
}
#endif
