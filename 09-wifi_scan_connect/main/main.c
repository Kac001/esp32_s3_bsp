#include <stdio.h>
#include "esp32_s3_szp.h"
#include "app_ui.h"
#include "nvs_flash.h"


void app_main(void)
{
    // 初始化 NVS（非易失性存储）闪存
    esp_err_t ret = nvs_flash_init(); // 调用 nvs_flash_init() 函数初始化 NVS，返回结果存储在 ret 中

    // 检查初始化是否成功
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
         // 如果没有可用的页（ESP_ERR_NVS_NO_FREE_PAGES）或发现新版本（ESP_ERR_NVS_NEW_VERSION_FOUND）
        ESP_ERROR_CHECK(nvs_flash_erase());// 清除 NVS 中的所有数据
        ret = nvs_flash_init();// 重新初始化 NVS
    }
    // 检查初始化结果是否成功
    ESP_ERROR_CHECK( ret );// 如果 ret 不是 ESP_OK，则报告错误并终止程序

    bsp_i2c_init();  // I2C初始化
    pca9557_init();  // IO扩展芯片初始化
    bsp_lvgl_start(); // 初始化液晶屏lvgl接口
    app_wifi_connect(); // 运行wifi连接程序


    /*程序运行流程
    1.app_wifi_connect wifi连接页面
        构建wifi搜索页面
        调用wifi_scan函数 搜索wifi
        循环遍历wifi列表，在页面中显示，点击事件函数=list_btn_cb
        启动wifi连接任务wifi_connect，队列等待xQueueWifiAccount队列消息
        调用list_btn_cb 输入密码界面
    2.list_btn_cb  输入密码界面
        返回按钮事件函数=btn_back_cb
        连接按钮事件函数=btn_connect_cb
        删除密码按钮函数=btn_del
    3.btn_connect_cb 连接按钮处理函数
        调用lv_wifi_connect Wi-Fi连接中页面
        发送QueueWifiAccount队列消息，执行Wi-Fi连接流程
    4.wifi_connect Wi-Fi连接流程
    */
}
