/*
 * bsp_lvgl.c — LVGL 移植层（esp_lvgl_port）
 */
#include "esp_check.h"
#include "esp_log.h"

#include "bsp_lvgl.h"
#include "bsp_display.h"
#include "bsp_touch.h"
#include "bsp_board.h"
#include "bsp_lcd_config_internal.h"

static const char *TAG = "bsp_lvgl";

static lv_disp_t *s_disp = NULL;
static lv_indev_t *s_touch_indev = NULL;
static bool s_lvgl_port_inited = false;

lv_disp_t *bsp_lvgl_init(void)
{
    if (s_disp != NULL) {
        return s_disp;
    }

    /* LVGL 移植环境（tick + timer 任务）；lvgl_port_init 非幂等，只允许一次 */
    if (!s_lvgl_port_inited) {
        const lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();
        if (lvgl_port_init(&lvgl_cfg) != ESP_OK) {
            ESP_LOGE(TAG, "lvgl_port_init failed");
            return NULL;
        }
        s_lvgl_port_inited = true;
    }

    /* 显示屏 */
    if (bsp_display_init() != ESP_OK) {
        ESP_LOGE(TAG, "display init failed");
        return NULL;
    }

    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = bsp_display_io(),
        .panel_handle = bsp_display_panel(),
        .buffer_size = BSP_LCD_H_RES * BSP_LCD_DRAW_BUF_HEIGHT,
        .double_buffer = true,
        .hres = BSP_LCD_H_RES,
        .vres = BSP_LCD_V_RES,
        .monochrome = false,
        /* 必须与 bsp_display_init() 中的 swap_xy / mirror 一致 */
        .rotation = {
            .swap_xy = BSP_LCD_CFG_SWAP_XY,
            .mirror_x = BSP_LCD_CFG_MIRROR_X,
            .mirror_y = BSP_LCD_CFG_MIRROR_Y,
        },
        .flags = {
            .buff_dma = true,      /* 注意：dma 与 spiram 不能同时为 true */
            .buff_spiram = false,
        },
    };
    s_disp = lvgl_port_add_disp(&disp_cfg);
    if (s_disp == NULL) {
        ESP_LOGE(TAG, "lvgl_port_add_disp failed");
        return NULL;
    }

#if defined(CONFIG_BSP_ENABLE_TOUCH)
    /* 触摸屏（失败不致命：界面仍可显示，只是没有触摸输入） */
    esp_lcd_touch_handle_t tp = NULL;
    if (bsp_touch_new(&tp) != ESP_OK) {
        ESP_LOGW(TAG, "touch init failed, continue without touch");
    } else {
        const lvgl_port_touch_cfg_t touch_cfg = {
            .disp = s_disp,
            .handle = tp,
        };
        s_touch_indev = lvgl_port_add_touch(&touch_cfg);
    }
#endif

    ESP_LOGI(TAG, "LVGL ready (display %dx%d, touch %s)", BSP_LCD_H_RES, BSP_LCD_V_RES,
             s_touch_indev ? "enabled" : "disabled");
    return s_disp;
}

lv_indev_t *bsp_lvgl_touch_indev(void)
{
    return s_touch_indev;
}
