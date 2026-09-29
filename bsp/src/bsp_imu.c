/*
 * bsp_imu.c — QMI8658 六轴姿态传感器
 *
 * 采用 demo 14-handheld 的完整版初始化（返回 esp_err_t、带运动检测配置），
 * 修正了旧版“读不到 ID 就死循环”的问题。
 */
#include <math.h>
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "bsp_imu.h"
#include "bsp_i2c.h"
#include "bsp_board_pins.h"

static const char *TAG = "bsp_imu";

/* QMI8658 寄存器地址（节选） */
enum {
    QMI8658_WHO_AM_I = 0x00,
    QMI8658_CTRL1 = 0x02,
    QMI8658_CTRL2 = 0x03,
    QMI8658_CTRL3 = 0x04,
    QMI8658_CTRL7 = 0x08,
    QMI8658_CTRL8 = 0x09,
    QMI8658_CTRL9 = 0x0A,
    QMI8658_CATL1_L = 0x0B,
    QMI8658_CATL1_H = 0x0C,
    QMI8658_CATL2_L = 0x0D,
    QMI8658_CATL2_H = 0x0E,
    QMI8658_CATL3_L = 0x0F,
    QMI8658_CATL3_H = 0x10,
    QMI8658_CATL4_L = 0x11,
    QMI8658_CATL4_H = 0x12,
    QMI8658_STATUS0 = 0x2E,
    QMI8658_STATUS1 = 0x2F,
    QMI8658_AX_L = 0x35,
    QMI8658_RESET = 0x60,
};

#define QMI8658_WHO_AM_I_VALUE   0x05

static i2c_master_dev_handle_t s_dev = NULL;

static esp_err_t qmi_read(uint8_t reg, uint8_t *data, size_t len)
{
    return i2c_master_transmit_receive(s_dev, &reg, 1, data, len, pdMS_TO_TICKS(1000));
}

static esp_err_t qmi_write_byte(uint8_t reg, uint8_t value)
{
    uint8_t buf[2] = { reg, value };
    return i2c_master_transmit(s_dev, buf, sizeof(buf), pdMS_TO_TICKS(1000));
}

esp_err_t bsp_imu_init(void)
{
    ESP_RETURN_ON_ERROR(bsp_i2c_init(), TAG, "I2C bus init failed");
    if (s_dev == NULL) {
        ESP_RETURN_ON_ERROR(bsp_i2c_device_add(BSP_QMI8658_ADDR, &s_dev), TAG, "QMI8658 add device failed");
    }

    uint8_t id = 0;
    int retry = 3;
    while (retry-- > 0) {
        if (qmi_read(QMI8658_WHO_AM_I, &id, 1) == ESP_OK && id == QMI8658_WHO_AM_I_VALUE) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    ESP_RETURN_ON_FALSE(id == QMI8658_WHO_AM_I_VALUE, ESP_ERR_NOT_FOUND, TAG,
                        "QMI8658 not found (WHO_AM_I=0x%02x)", id);

    qmi_write_byte(QMI8658_RESET, 0xb0);            /* 软复位 */
    vTaskDelay(pdMS_TO_TICKS(10));

    /* 运动检测配置（AnyMotion / NoMotion） */
    qmi_write_byte(QMI8658_CATL1_L, 1);
    qmi_write_byte(QMI8658_CATL1_H, 1);
    qmi_write_byte(QMI8658_CATL2_L, 1);
    qmi_write_byte(QMI8658_CATL2_H, 1);
    qmi_write_byte(QMI8658_CATL3_L, 1);
    qmi_write_byte(QMI8658_CATL3_H, 1);
    qmi_write_byte(QMI8658_CATL4_L, 0x77);          /* MOTION_MODE_CTRL */
    qmi_write_byte(QMI8658_CATL4_H, 0x01);          /* 第 1 条命令 */
    qmi_write_byte(QMI8658_CTRL9, 0x0E);            /* CTRL_CMD_CONFIGURE_MOTION */

    qmi_write_byte(QMI8658_CATL1_L, 1);             /* AnyMotionWindow */
    qmi_write_byte(QMI8658_CATL1_H, 1);             /* NoMotionWindow */
    qmi_write_byte(QMI8658_CATL2_L, 0xE8);          /* SigMotionWaitWindow[7:0] */
    qmi_write_byte(QMI8658_CATL2_H, 0x03);
    qmi_write_byte(QMI8658_CATL3_L, 0xE8);          /* SigMotionConfirmWindow[7:0] */
    qmi_write_byte(QMI8658_CATL3_H, 0x03);
    qmi_write_byte(QMI8658_CATL4_H, 0x02);          /* 第 2 条命令 */
    qmi_write_byte(QMI8658_CTRL9, 0x0E);

    qmi_write_byte(QMI8658_CTRL1, 0x40);            /* 地址自增 */
    qmi_write_byte(QMI8658_CTRL7, 0x03);            /* 使能加速度计 + 陀螺仪 */
    qmi_write_byte(QMI8658_CTRL2, 0x95);            /* ACC 4g 250Hz */
    qmi_write_byte(QMI8658_CTRL3, 0xd5);            /* GYR 512dps 250Hz */
    qmi_write_byte(QMI8658_CTRL8, 0x0E);            /* 运动检测事件使能 */

    ESP_LOGI(TAG, "QMI8658 ready (WHO_AM_I=0x%02x)", id);
    return ESP_OK;
}

void bsp_imu_close(void)
{
    qmi_write_byte(QMI8658_CTRL1, 0x01);
}

esp_err_t bsp_imu_read(bsp_imu_data_t *data)
{
    ESP_RETURN_ON_FALSE(data != NULL, ESP_ERR_INVALID_ARG, TAG, "data is NULL");

    uint8_t status = 0;
    ESP_RETURN_ON_ERROR(qmi_read(QMI8658_STATUS0, &status, 1), TAG, "read STATUS0 failed");
    if ((status & 0x03) == 0) {
        return ESP_ERR_INVALID_STATE;   /* 数据未就绪 */
    }

    uint8_t raw[12];
    ESP_RETURN_ON_ERROR(qmi_read(QMI8658_AX_L, raw, sizeof(raw)), TAG, "read data failed");
    int16_t buf[6];
    for (int i = 0; i < 6; i++) {
        buf[i] = (int16_t)((uint16_t)raw[2 * i] | ((uint16_t)raw[2 * i + 1] << 8));
    }
    data->acc_x = buf[0];
    data->acc_y = buf[1];
    data->acc_z = buf[2];
    data->gyr_x = buf[3];
    data->gyr_y = buf[4];
    data->gyr_z = buf[5];

    /* 由加速度计解算倾角（弧度 -> 角度） */
    float ax = (float)data->acc_x, ay = (float)data->acc_y, az = (float)data->acc_z;
    data->angle_x = atanf(ax / sqrtf(ay * ay + az * az)) * 57.29578f;
    data->angle_y = atanf(ay / sqrtf(ax * ax + az * az)) * 57.29578f;
    data->angle_z = atanf(sqrtf(ax * ax + ay * ay) / az) * 57.29578f;
    return ESP_OK;
}

uint8_t bsp_imu_motion_status(void)
{
    uint8_t status = 0;
    qmi_read(QMI8658_STATUS1, &status, 1);
    return status;
}
