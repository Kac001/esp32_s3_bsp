/*
 * bsp_smoke — BSP 自检示例
 *
 * 这是一个【自设计】的最小界面：上电自检并显示各外设状态。
 * 仅演示 BSP 的正确用法；真实应用的界面请自行设计，不要照搬本示例或 demo。
 */
#include <stdio.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "esp_lvgl_port.h"

#include "bsp_board.h"
#include "bsp_display.h"
#include "bsp_lvgl.h"
#include "bsp_audio.h"
#include "bsp_camera.h"
#include "bsp_storage.h"
#include "bsp_imu.h"

LV_FONT_DECLARE(font_ui_cn_20);

static const char *TAG = "bsp_smoke";

typedef enum {
    ST_OK = 0,      /* 正常 */
    ST_FAIL,        /* 失败 */
    ST_NA,          /* 未启用 */
    ST_NOCARD,      /* 无卡 */
} status_t;

static const char *status_text(status_t st)
{
    switch (st) {
    case ST_OK:     return "正常";
    case ST_FAIL:   return "失败";
    case ST_NOCARD: return "无卡";
    default:        return "未启用";
    }
}

static lv_color_t status_color(status_t st)
{
    switch (st) {
    case ST_OK:     return lv_color_hex(0x3fb950);
    case ST_NOCARD: return lv_color_hex(0xd29922);
    default:        return lv_color_hex(0xf85149);
    }
}

static void label_create(lv_obj_t *parent, const char *text, lv_coord_t x, lv_coord_t y,
                         lv_color_t color, lv_text_align_t align, lv_coord_t width)
{
    lv_obj_t *lab = lv_label_create(parent);
    lv_label_set_text(lab, text);
    lv_obj_set_style_text_font(lab, &font_ui_cn_20, 0);
    lv_obj_set_style_text_color(lab, color, 0);
    lv_obj_set_width(lab, width);
    lv_obj_set_style_text_align(lab, align, 0);
    lv_obj_set_pos(lab, x, y);
}

/* 自检界面：一行一个外设，显示检测结果 */
static void build_ui(const char *names[], const status_t status[], int count)
{
    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x0d1117), 0);
    lv_obj_set_style_text_line_space(scr, 4, 0);

    /* 顶栏 */
    lv_obj_t *top = lv_obj_create(scr);
    lv_obj_remove_style_all(top);
    lv_obj_set_size(top, BSP_LCD_H_RES, 38);
    lv_obj_set_pos(top, 0, 0);
    lv_obj_set_style_bg_color(top, lv_color_hex(0x1f6feb), 0);
    lv_obj_set_style_bg_opa(top, LV_OPA_COVER, 0);
    label_create(top, "BSP 自检", 12, 5, lv_color_white(), LV_TEXT_ALIGN_LEFT, 140);
    label_create(top, bsp_board_name(), 120, 8, lv_color_hex(0xdce7ff), LV_TEXT_ALIGN_RIGHT, 188);

    /* 状态行 */
    for (int i = 0; i < count; i++) {
        lv_coord_t y = 50 + i * 26;
        label_create(scr, names[i], 16, y, lv_color_hex(0xc9d1d9), LV_TEXT_ALIGN_LEFT, 120);
        label_create(scr, status_text(status[i]), 150, y, status_color(status[i]), LV_TEXT_ALIGN_RIGHT, 154);
        lv_obj_t *line = lv_obj_create(scr);
        lv_obj_remove_style_all(line);
        lv_obj_set_size(line, BSP_LCD_H_RES - 32, 1);
        lv_obj_set_pos(line, 16, y + 24);
        lv_obj_set_style_bg_color(line, lv_color_hex(0x21262d), 0);
        lv_obj_set_style_bg_opa(line, LV_OPA_COVER, 0);
    }

    /* 底部提示 */
    label_create(scr, "界面为示例，请自行设计", 0, BSP_LCD_V_RES - 30,
                 lv_color_hex(0x8b949e), LV_TEXT_ALIGN_CENTER, BSP_LCD_H_RES);
}

void app_main(void)
{
    ESP_LOGI(TAG, "board: %s rev %s", bsp_board_name(), bsp_board_revision());

    /* ---- 硬件自检（顺序即依赖关系） ---- */
    bsp_board_init();
    bsp_display_init();
    bsp_display_fill(0x0000);

    status_t st[6];
    const char *names[6] = { "显示", "触摸", "音频", "摄像头", "存储", "传感器" };

    st[0] = ST_OK;                                              /* 显示 */
    st[2] = (bsp_audio_init() == ESP_OK) ? ST_OK : ST_FAIL;     /* 音频 */
    st[3] = (bsp_camera_init(NULL) == ESP_OK) ? ST_OK : ST_FAIL;/* 摄像头 */

    esp_err_t r_spiffs = bsp_spiffs_mount(NULL);
    esp_err_t r_sd = bsp_sdcard_mount();
    if (r_spiffs == ESP_OK) {
        st[4] = ST_OK;
    } else if (r_sd == ESP_OK) {
        st[4] = ST_OK;
    } else {
        st[4] = ST_NOCARD;
    }
    st[5] = (bsp_imu_init() == ESP_OK) ? ST_OK : ST_FAIL;       /* 传感器 */

    /* ---- LVGL + 触摸 + 界面 ---- */
    lv_disp_t *disp = bsp_lvgl_init();
    if (disp == NULL) {
        ESP_LOGE(TAG, "LVGL init failed");
        return;
    }
    st[1] = (bsp_lvgl_touch_indev() != NULL) ? ST_OK : ST_NA;   /* 触摸 */

    if (lvgl_port_lock(portMAX_DELAY)) {
        build_ui(names, st, 6);
        lvgl_port_unlock();
    }

    ESP_LOGI(TAG, "self-test done: %s %s %s %s %s %s",
             status_text(st[0]), status_text(st[1]), status_text(st[2]),
             status_text(st[3]), status_text(st[4]), status_text(st[5]));
}
