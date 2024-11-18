#include <stdio.h>  // 包含标准输入输出库，提供printf等函数
#include <inttypes.h>  // 包含用于整数类型的定义和宏，例如PRIu32
#include "sdkconfig.h" // 包含SDK配置文件，定义了项 目的配置
#include "freertos/FreeRTOS.h" // 包含FreeRTOS相关的头文件
#include "freertos/task.h"  // 包含FreeRTOS任务相关的头文件
#include "esp_chip_info.h" // 包含ESP芯片信息的头文件
#include "esp_flash.h" // 包含ESP闪存相关的头文件
#include "esp_system.h" // 包含ESP系统相关的头文件

void app_main(void) // 定义应用程序的主入口函数
{
    printf("Hello world!\n");  // 输出“Hello world!”到控制台

    /* 打印芯片信息 */
    esp_chip_info_t chip_info; // 定义一个esp_chip_info_t结构体变量用于存储芯片信息
    uint32_t flash_size; // 定义一个32位无符号整数变量用于存储闪存大小
    esp_chip_info(&chip_info); // 获取芯片信息并存储到chip_info结构体变量中
    printf("This is %s chip with %d CPU core(s), %s%s%s%s, ",
           CONFIG_IDF_TARGET, // 输出芯片的目标配置,来自于#include "sdkconfig.h"，读取sdkconfig文件中变量
           chip_info.cores,   // 输出CPU核心数量
           (chip_info.features & CHIP_FEATURE_WIFI_BGN) ? "WiFi/" : "",  // 检查是否支持WiFi并输出
           (chip_info.features & CHIP_FEATURE_BT) ? "BT" : "", // 检查是否支持蓝牙并输出
           (chip_info.features & CHIP_FEATURE_BLE) ? "BLE" : "",  // 检查是否支持BLE并输出
           (chip_info.features & CHIP_FEATURE_IEEE802154) ? ", 802.15.4 (Zigbee/Thread)" : ""); // 检查是否支持802.15.4并输出

    unsigned major_rev = chip_info.revision / 100; // 计算主要版本号
    unsigned minor_rev = chip_info.revision % 100; // 计算次要版本号
    printf("silicon revision v%d.%d, ", major_rev, minor_rev); // 输出芯片的硅片版本

    // 获取闪存大小并检查是否成功
    if(esp_flash_get_size(NULL, &flash_size) != ESP_OK) {
        printf("Get flash size failed");  // 输出获取闪存大小失败的提示
        return;
    }

    // 输出闪存大小信息
    printf("%" PRIu32 "MB %s flash\n", flash_size / (uint32_t)(1024 * 1024), // 输出闪存大小，转换为MB
           (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external"); // 检查闪存是嵌入式还是外部

    printf("Minimum free heap size: %" PRIu32 " bytes\n", esp_get_minimum_free_heap_size()); // 输出最小可用堆内存大小

    // 倒计时重启
    for (int i = 10; i >= 0; i--) {  // 从10倒计时到0
        printf("Restarting in %d seconds...\n", i);  // 输出倒计时信息
        vTaskDelay(1000 / portTICK_PERIOD_MS);  // 延迟1秒（1000毫秒）
    }
    printf("Restarting now.\n"); // 输出重启信息
    fflush(stdout); //清空输出缓冲区，运行后立即输出printf 输出的内容(printf需要换行符\n才会立即输出，这里用于保险作用)
    esp_restart(); // 重启ESP设备
}
