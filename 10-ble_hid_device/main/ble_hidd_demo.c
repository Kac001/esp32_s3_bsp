/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <stdio.h> // 包含标准输入输出库
#include <stdlib.h>// 包含标准库，提供内存管理、随机数生成等功能
#include <string.h>// 包含字符串处理库
#include "freertos/FreeRTOS.h"// 包含 FreeRTOS 操作系统头文件
#include "freertos/task.h"// 包含 FreeRTOS 任务管理相关头文件
#include "freertos/event_groups.h"// 包含 FreeRTOS 事件组相关头文件
#include "esp_system.h"// 包含 ESP32 系统相关功能头文件
#include "esp_wifi.h"// 包含 ESP32 Wi-Fi 相关功能头文件
#include "esp_event.h"// 包含 ESP32 事件处理相关头文件
#include "esp_log.h"// 包含非易失性存储（NVS）相关头文件
#include "nvs_flash.h"
#include "esp_bt.h"

#include "esp_hidd_prf_api.h"
#include "esp_bt_defs.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_gatt_defs.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "driver/gpio.h"
#include "hid_dev.h"

#include "esp_lvgl_port.h"

/**
 * Brief:
 * This example Implemented BLE HID device profile related functions, in which the HID device
 * has 4 Reports (1 is mouse, 2 is keyboard and LED, 3 is Consumer Devices, 4 is Vendor devices).
 * Users can choose different reports according to their own application scenarios.
 * BLE HID profile inheritance and USB HID class.
 */

/**
 * Note:
 * 1. Win10 does not support vendor report , So SUPPORT_REPORT_VENDOR is always set to FALSE, it defines in hidd_le_prf_int.h
 * 2. Update connection parameters are not allowed during iPhone HID encryption, slave turns
 * off the ability to automatically update connection parameters during encryption.
 * 3. After our HID device is connected, the iPhones write 1 to the Report Characteristic Configuration Descriptor,
 * even if the HID encryption is not completed. This should actually be written 1 after the HID encryption is completed.
 * we modify the permissions of the Report Characteristic Configuration Descriptor to `ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE_ENCRYPTED`.
 * if you got `GATT_INSUF_ENCRYPTION` error, please ignore.
 */

#define HID_DEMO_TAG "HID_DEMO"


static uint16_t hid_conn_id = 0;
static bool sec_conn = false;

#define CHAR_DECLARATION_SIZE   (sizeof(uint8_t))

static void hidd_event_callback(esp_hidd_cb_event_t event, esp_hidd_cb_param_t *param);

#define HIDD_DEVICE_NAME            "HID"
static uint8_t hidd_service_uuid128[] = {
    /* LSB <--------------------------------------------------------------------------------> MSB */
    //first uuid, 16bit, [12],[13] is the value
    0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0x12, 0x18, 0x00, 0x00,
};

static esp_ble_adv_data_t hidd_adv_data = {
    .set_scan_rsp = false,
    .include_name = true,
    .include_txpower = true,
    .min_interval = 0x0006, //slave connection min interval, Time = min_interval * 1.25 msec
    .max_interval = 0x0010, //slave connection max interval, Time = max_interval * 1.25 msec
    .appearance = 0x03c0,       //HID Generic,
    .manufacturer_len = 0,
    .p_manufacturer_data =  NULL,
    .service_data_len = 0,
    .p_service_data = NULL,
    .service_uuid_len = sizeof(hidd_service_uuid128),
    .p_service_uuid = hidd_service_uuid128,
    .flag = 0x6,
};

static esp_ble_adv_params_t hidd_adv_params = {
    .adv_int_min        = 0x20,
    .adv_int_max        = 0x30,
    .adv_type           = ADV_TYPE_IND,
    .own_addr_type      = BLE_ADDR_TYPE_PUBLIC,
    //.peer_addr            =
    //.peer_addr_type       =
    .channel_map        = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};


static void hidd_event_callback(esp_hidd_cb_event_t event, esp_hidd_cb_param_t *param)
{
    switch(event) {
        case ESP_HIDD_EVENT_REG_FINISH: {
            if (param->init_finish.state == ESP_HIDD_INIT_OK) {
                //esp_bd_addr_t rand_addr = {0x04,0x11,0x11,0x11,0x11,0x05};
                esp_ble_gap_set_device_name(HIDD_DEVICE_NAME);
                esp_ble_gap_config_adv_data(&hidd_adv_data);

            }
            break;
        }
        case ESP_BAT_EVENT_REG: {
            break;
        }
        case ESP_HIDD_EVENT_DEINIT_FINISH:
	     break;
		case ESP_HIDD_EVENT_BLE_CONNECT: {
            ESP_LOGI(HID_DEMO_TAG, "ESP_HIDD_EVENT_BLE_CONNECT");
            hid_conn_id = param->connect.conn_id;
            break;
        }
        case ESP_HIDD_EVENT_BLE_DISCONNECT: {
            sec_conn = false;
            ESP_LOGI(HID_DEMO_TAG, "ESP_HIDD_EVENT_BLE_DISCONNECT");
            esp_ble_gap_start_advertising(&hidd_adv_params);
            break;
        }
        case ESP_HIDD_EVENT_BLE_VENDOR_REPORT_WRITE_EVT: {
            ESP_LOGI(HID_DEMO_TAG, "%s, ESP_HIDD_EVENT_BLE_VENDOR_REPORT_WRITE_EVT", __func__);
            ESP_LOG_BUFFER_HEX(HID_DEMO_TAG, param->vendor_write.data, param->vendor_write.length);
            break;
        }
        case ESP_HIDD_EVENT_BLE_LED_REPORT_WRITE_EVT: {
            ESP_LOGI(HID_DEMO_TAG, "ESP_HIDD_EVENT_BLE_LED_REPORT_WRITE_EVT");
            ESP_LOG_BUFFER_HEX(HID_DEMO_TAG, param->led_write.data, param->led_write.length);
            break;
        }
        default:
            break;
    }
    return;
}

static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    switch (event) {
    case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
        esp_ble_gap_start_advertising(&hidd_adv_params);
        break;
     case ESP_GAP_BLE_SEC_REQ_EVT:
        for(int i = 0; i < ESP_BD_ADDR_LEN; i++) {
             ESP_LOGD(HID_DEMO_TAG, "%x:",param->ble_security.ble_req.bd_addr[i]);
        }
        esp_ble_gap_security_rsp(param->ble_security.ble_req.bd_addr, true);
	 break;
     case ESP_GAP_BLE_AUTH_CMPL_EVT:
        sec_conn = true;
        esp_bd_addr_t bd_addr;
        memcpy(bd_addr, param->ble_security.auth_cmpl.bd_addr, sizeof(esp_bd_addr_t));
        ESP_LOGI(HID_DEMO_TAG, "remote BD_ADDR: %08x%04x",\
                (bd_addr[0] << 24) + (bd_addr[1] << 16) + (bd_addr[2] << 8) + bd_addr[3],
                (bd_addr[4] << 8) + bd_addr[5]);
        ESP_LOGI(HID_DEMO_TAG, "address type = %d", param->ble_security.auth_cmpl.addr_type);
        ESP_LOGI(HID_DEMO_TAG, "pair status = %s",param->ble_security.auth_cmpl.success ? "success" : "fail");
        if(!param->ble_security.auth_cmpl.success) {
            ESP_LOGE(HID_DEMO_TAG, "fail reason = 0x%x",param->ble_security.auth_cmpl.fail_reason);
        }
        break;
    default:
        break;
    }
}

// 启动蓝牙 HID 设备
void bt_hid_start(void)
{
    esp_err_t ret;// 返回值变量

    // 初始化 NVS（非易失性存储）
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {// 检查 NVS 初始化错误
        ESP_ERROR_CHECK(nvs_flash_erase());// 清除 NVS
        ret = nvs_flash_init();// 重新初始化 NVS
    }
    ESP_ERROR_CHECK( ret ); // 检查错误

    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));// 释放经典蓝牙内存

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT(); // 默认蓝牙控制器配置
    ret = esp_bt_controller_init(&bt_cfg);// 初始化蓝牙控制器
    if (ret) {
        ESP_LOGE(HID_DEMO_TAG, "%s initialize controller failed\n", __func__);// 日志输出错误
        return;// 退出函数
    }

    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE); // 启用 BLE 模式
    if (ret) {
        ESP_LOGE(HID_DEMO_TAG, "%s enable controller failed\n", __func__); // 日志输出错误
        return;// 退出函数
    }

    ret = esp_bluedroid_init(); // 初始化 Bluedroid
    if (ret) {
        ESP_LOGE(HID_DEMO_TAG, "%s init bluedroid failed\n", __func__);// 日志输出错误
        return;// 退出函数
    }

    ret = esp_bluedroid_enable(); // 启用 Bluedroid
    if (ret) {
        ESP_LOGE(HID_DEMO_TAG, "%s init bluedroid failed\n", __func__);// 日志输出错误
        return;// 退出函数
    }

    if((ret = esp_hidd_profile_init()) != ESP_OK) {// 初始化 HID 配置
        ESP_LOGE(HID_DEMO_TAG, "%s init bluedroid failed\n", __func__); // 日志输出错误
    }

    /// 注册 GAP 事件处理函数
    esp_ble_gap_register_callback(gap_event_handler);
    esp_hidd_register_callbacks(hidd_even t_callback);// 注册 HID 事件回调函数

    /* 设置安全参数*/
    esp_ble_auth_req_t auth_req = ESP_LE_AUTH_BOND;      // 配对后进行绑定
    esp_ble_io_cap_t iocap = ESP_IO_CAP_NONE;           // 设置 IO 能力为无输入无输出
    uint8_t key_size = 16;      //密钥大小应为 7~16 字节
    uint8_t init_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;// 初始化密钥
    uint8_t rsp_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;// 响应密钥
    esp_ble_gap_set_security_param(ESP_BLE_SM_AUTHEN_REQ_MODE, &auth_req, sizeof(uint8_t));// 设置认证请求模式
    esp_ble_gap_set_security_param(ESP_BLE_SM_IOCAP_MODE, &iocap, sizeof(uint8_t));// 设置 IO 能力模式
    esp_ble_gap_set_security_param(ESP_BLE_SM_MAX_KEY_SIZE, &key_size, sizeof(uint8_t)); // 设置最大密钥大小
    /* If your BLE device act as a Slave, the init_key means you hope which types of key of the master should distribute to you,
    and the response key means which key you can distribute to the Master;
    If your BLE device act as a master, the response key means you hope which types of key of the slave should distribute to you,
    and the init key means which key you can distribute to the slave. */
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_INIT_KEY, &init_key, sizeof(uint8_t));// 设置初始化密钥
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_RSP_KEY, &rsp_key, sizeof(uint8_t));// 设置响应密钥 
}

static void btn2_event_handler(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if (sec_conn) {
        if(code == LV_EVENT_PRESSING) {
            esp_hidd_send_consumer_value(hid_conn_id, HID_CONSUMER_VOLUME_UP, true);
            ESP_LOGI(HID_DEMO_TAG, "UP LV_EVENT_CLICKED");
        }
        else if(code == LV_EVENT_RELEASED) {
            esp_hidd_send_consumer_value(hid_conn_id, HID_CONSUMER_VOLUME_UP, false);
            ESP_LOGI(HID_DEMO_TAG, "UP LV_EVENT_RELEASED");
        }
    }
}

static void btn1_event_handler(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if (sec_conn) {
        if(code == LV_EVENT_PRESSING) {
            esp_hidd_send_consumer_value(hid_conn_id, HID_CONSUMER_VOLUME_DOWN, true);
            ESP_LOGI(HID_DEMO_TAG, "DOWN LV_EVENT_CLICKED");
        }
        else if(code == LV_EVENT_RELEASED) {
            esp_hidd_send_consumer_value(hid_conn_id, HID_CONSUMER_VOLUME_DOWN, false);
            ESP_LOGI(HID_DEMO_TAG, "DOWN LV_EVENT_RELEASED");
        }
    }
}

// 运行蓝牙HID控制程序
void app_hid_ctrl(void)
{
    lvgl_port_lock(0);// 锁定 LVGL 端口

    lv_obj_t * label;// 标签对象指针

    // 创建按钮 音量-
    lv_obj_t * btn1 = lv_btn_create(lv_scr_act());// 创建按钮  音量-
    lv_obj_add_event_cb(btn1, btn1_event_handler, LV_EVENT_ALL, NULL);// 添加事件回调
    lv_obj_align(btn1, LV_ALIGN_CENTER, -50, 0); // 对齐按钮位置
    lv_obj_set_size(btn1, 80, 80);// 设置按钮大小

    label = lv_label_create(btn1);// 创建标签
    lv_label_set_text(label, LV_SYMBOL_VOLUME_MID); // 设置标签文本
    lv_obj_set_style_text_font(label, &lv_font_montserrat_20, 0); // 设置字体
    lv_obj_center(label);// 标签居中


    // 创建按钮 音量+
    lv_obj_t * btn2 = lv_btn_create(lv_scr_act());// 创建按钮  音量+
    lv_obj_add_event_cb(btn2, btn2_event_handler, LV_EVENT_ALL, NULL);// 添加事件回调
    lv_obj_align(btn2, LV_ALIGN_CENTER, 50, 0);// 对齐按钮位置
    lv_obj_set_size(btn2, 80, 80);// 设置按钮大小

    label = lv_label_create(btn2); // 创建标签
    lv_label_set_text(label, LV_SYMBOL_VOLUME_MAX); // 设置标签文本
    lv_obj_set_style_text_font(label, &lv_font_montserrat_20, 0); // 设置字体
    lv_obj_center(label);// 标签居中

    lvgl_port_unlock(); // 解锁 LVGL 端口

    bt_hid_start();// 启动蓝牙 HID 设备
}

