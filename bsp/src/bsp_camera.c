/*
 * bsp_camera.c — DVP 摄像头（esp32-camera）
 */
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/ledc.h"

#include "bsp_camera.h"
#include "bsp_board.h"
#include "bsp_io_expander.h"
#include "bsp_i2c.h"

static const char *TAG = "bsp_cam";

static bool s_inited = false;

esp_err_t bsp_camera_power(bool on)
{
    /* DVP_PWDN：1=休眠 0=工作 */
    return bsp_ioe_write(BSP_IOE_CAM_PWDN, !on);
}

esp_err_t bsp_camera_init(const bsp_camera_config_t *cfg)
{
    if (s_inited) {
        return ESP_OK;
    }

    bsp_camera_config_t default_cfg = {
        .pixel_format = PIXFORMAT_RGB565,
        .frame_size = FRAMESIZE_QVGA,
        .fb_count = 2,
        .hmirror = true,
    };
    if (cfg == NULL) {
        cfg = &default_cfg;
    }

    ESP_RETURN_ON_ERROR(bsp_camera_power(true), TAG, "camera power-on failed");
    vTaskDelay(pdMS_TO_TICKS(10));

    camera_config_t config = {
        .ledc_channel = LEDC_CHANNEL_1,     /* S3 用 APLL 产生 XCLK，此项不生效 */
        .ledc_timer = LEDC_TIMER_1,
        .pin_d0 = BSP_CAM_D0,
        .pin_d1 = BSP_CAM_D1,
        .pin_d2 = BSP_CAM_D2,
        .pin_d3 = BSP_CAM_D3,
        .pin_d4 = BSP_CAM_D4,
        .pin_d5 = BSP_CAM_D5,
        .pin_d6 = BSP_CAM_D6,
        .pin_d7 = BSP_CAM_D7,
        .pin_xclk = BSP_CAM_XCLK,
        .pin_pclk = BSP_CAM_PCLK,
        .pin_vsync = BSP_CAM_VSYNC,
        .pin_href = BSP_CAM_HREF,
        .pin_sccb_sda = -1,                 /* 复用已初始化的 I2C0 */
        .pin_sccb_scl = BSP_CAM_SIOC,
        .sccb_i2c_port = BSP_I2C_NUM,
        .pin_pwdn = BSP_CAM_PWDN,
        .pin_reset = BSP_CAM_RESET,
        .xclk_freq_hz = BSP_CAM_XCLK_FREQ_HZ,
        .pixel_format = cfg->pixel_format,
        .frame_size = cfg->frame_size,
        .jpeg_quality = 12,
        .fb_count = cfg->fb_count > 0 ? cfg->fb_count : 2,
        .fb_location = CAMERA_FB_IN_PSRAM,
        .grab_mode = CAMERA_GRAB_WHEN_EMPTY,
    };

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_camera_init failed: %s", esp_err_to_name(err));
        bsp_camera_power(false);
        return err;
    }

    sensor_t *s = esp_camera_sensor_get();
    if (s != NULL) {
        ESP_LOGI(TAG, "camera sensor PID=0x%02x", (unsigned)s->id.PID);
        if (s->id.PID == GC0308_PID) {
            s->set_hmirror(s, cfg->hmirror ? 1 : 0);
        }
    }

    s_inited = true;
    return ESP_OK;
}

esp_err_t bsp_camera_deinit(void)
{
    if (!s_inited) {
        return ESP_OK;
    }
    esp_err_t ret = esp_camera_deinit();
    s_inited = false;
    bsp_camera_power(false);
    return ret;
}
