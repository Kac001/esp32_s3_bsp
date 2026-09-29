/*
 * bsp_display.c — LCD 显示（ST7789 SPI + LEDC 背光）
 */
#include <string.h>
#include "esp_check.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "driver/ledc.h"
#include "driver/spi_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"

#include "bsp_display.h"
#include "bsp_board.h"
#include "bsp_io_expander.h"

static const char *TAG = "bsp_disp";

#include "bsp_lcd_config_internal.h"

static esp_lcd_panel_handle_t s_panel = NULL;
static esp_lcd_panel_io_handle_t s_io = NULL;
static bool s_inited = false;

/* 常驻行缓冲（内部 DMA RAM），避免异步传输期间缓冲被释放 */
static uint16_t *s_line_buf = NULL;

esp_err_t bsp_display_brightness_init(void)
{
    const ledc_channel_config_t bl_channel = {
        .gpio_num = BSP_LCD_BACKLIGHT,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = BSP_LCD_LEDC_CHANNEL,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = BSP_LCD_LEDC_TIMER,
        .duty = 0,
        .hpoint = 0,
        .flags.output_invert = BSP_LCD_CFG_BACKLIGHT_INVERT,
    };
    const ledc_timer_config_t bl_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .timer_num = BSP_LCD_LEDC_TIMER,
        .freq_hz = BSP_LCD_BACKLIGHT_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_RETURN_ON_ERROR(ledc_timer_config(&bl_timer), TAG, "ledc_timer_config failed");
    ESP_RETURN_ON_ERROR(ledc_channel_config(&bl_channel), TAG, "ledc_channel_config failed");
    return ESP_OK;
}

esp_err_t bsp_display_brightness_set(int percent)
{
    if (percent > 100) {
        percent = 100;
    } else if (percent < 0) {
        percent = 0;
    }
    uint32_t duty = (1023 * (uint32_t)percent) / 100;
    ESP_RETURN_ON_ERROR(ledc_set_duty(LEDC_LOW_SPEED_MODE, BSP_LCD_LEDC_CHANNEL, duty), TAG, "ledc_set_duty failed");
    return ledc_update_duty(LEDC_LOW_SPEED_MODE, BSP_LCD_LEDC_CHANNEL);
}

esp_err_t bsp_display_backlight_on(void)
{
    return bsp_display_brightness_set(100);
}

esp_err_t bsp_display_backlight_off(void)
{
    return bsp_display_brightness_set(0);
}

esp_err_t bsp_display_init(void)
{
    if (s_inited) {
        return ESP_OK;
    }

    ESP_RETURN_ON_ERROR(bsp_display_brightness_init(), TAG, "backlight init failed");

    ESP_LOGD(TAG, "Initialize SPI bus");
    const spi_bus_config_t buscfg = {
        .sclk_io_num = BSP_LCD_SPI_CLK,
        .mosi_io_num = BSP_LCD_SPI_MOSI,
        .miso_io_num = GPIO_NUM_NC,
        .quadwp_io_num = GPIO_NUM_NC,
        .quadhd_io_num = GPIO_NUM_NC,
        .max_transfer_sz = BSP_LCD_H_RES * 40 * sizeof(uint16_t),
    };
    ESP_RETURN_ON_ERROR(spi_bus_initialize(BSP_LCD_SPI_NUM, &buscfg, SPI_DMA_CH_AUTO), TAG, "SPI init failed");

    ESP_LOGD(TAG, "Install panel IO");
    const esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = BSP_LCD_DC,
        .cs_gpio_num = BSP_LCD_SPI_CS,
        .pclk_hz = BSP_LCD_CFG_PIXEL_CLOCK_HZ,
        .lcd_cmd_bits = BSP_LCD_CMD_BITS,
        .lcd_param_bits = BSP_LCD_PARAM_BITS,
        .spi_mode = BSP_LCD_SPI_MODE,
        .trans_queue_depth = 10,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)BSP_LCD_SPI_NUM, &io_config, &s_io),
                        TAG, "New panel IO failed");

    /* LCD_CS 走 PCA9557：面板初始化前置低，保持选中 */
    ESP_RETURN_ON_ERROR(bsp_ioe_write(BSP_IOE_LCD_CS, false), TAG, "LCD_CS assert failed");

    ESP_LOGD(TAG, "Install ST7789 driver");
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = BSP_LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = BSP_LCD_BITS_PER_PIXEL,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_st7789(s_io, &panel_config, &s_panel), TAG, "New panel failed");

    esp_lcd_panel_reset(s_panel);
    esp_lcd_panel_init(s_panel);
    esp_lcd_panel_invert_color(s_panel, BSP_LCD_CFG_INVERT);
    esp_lcd_panel_swap_xy(s_panel, BSP_LCD_CFG_SWAP_XY);
    esp_lcd_panel_mirror(s_panel, BSP_LCD_CFG_MIRROR_X, BSP_LCD_CFG_MIRROR_Y);
    esp_lcd_panel_disp_on_off(s_panel, true);

    s_line_buf = (uint16_t *)heap_caps_malloc(BSP_LCD_H_RES * sizeof(uint16_t),
                                              MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    if (s_line_buf == NULL) {
        ESP_LOGW(TAG, "line buffer alloc failed, fill() unavailable");
    }

    s_inited = true;
    ESP_LOGI(TAG, "ST7789 %dx%d ready (SPI%d @%uHz, swap_xy=%d mirror=%d,%d invert=%d)",
             BSP_LCD_H_RES, BSP_LCD_V_RES, BSP_LCD_SPI_NUM, (unsigned)BSP_LCD_CFG_PIXEL_CLOCK_HZ,
             BSP_LCD_CFG_SWAP_XY, BSP_LCD_CFG_MIRROR_X, BSP_LCD_CFG_MIRROR_Y, BSP_LCD_CFG_INVERT);
    return ESP_OK;
}

esp_lcd_panel_handle_t bsp_display_panel(void)
{
    return s_panel;
}

esp_lcd_panel_io_handle_t bsp_display_io(void)
{
    return s_io;
}

esp_err_t bsp_display_fill(uint16_t rgb565)
{
    ESP_RETURN_ON_FALSE(s_panel && s_line_buf, ESP_ERR_INVALID_STATE, TAG, "call bsp_display_init() first");

    for (int x = 0; x < BSP_LCD_H_RES; x++) {
        s_line_buf[x] = rgb565;
    }
    for (int y = 0; y < BSP_LCD_V_RES; y++) {
        esp_lcd_panel_draw_bitmap(s_panel, 0, y, BSP_LCD_H_RES, y + 1, s_line_buf);
    }
    return ESP_OK;
}

esp_err_t bsp_display_draw_bitmap(int x_start, int y_start, int x_end, int y_end, const void *color_data)
{
    ESP_RETURN_ON_FALSE(s_panel, ESP_ERR_INVALID_STATE, TAG, "call bsp_display_init() first");
    ESP_RETURN_ON_FALSE(color_data != NULL, ESP_ERR_INVALID_ARG, TAG, "color_data is NULL");
    /* 注意：color_data 须在传输完成前保持有效（静态 const 图像数组可直接使用） */
    return esp_lcd_panel_draw_bitmap(s_panel, x_start, y_start, x_end, y_end, color_data);
}
