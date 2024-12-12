/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h> // 引入标准输入输出库
#include <string.h> // 引入字符串处理库
#include "sdkconfig.h" // 包含 SDK 配置
#include "freertos/FreeRTOS.h"  // 引入 FreeRTOS 相关头文件
#include "freertos/task.h" // 引入 FreeRTOS 任务管理相关头文件
#include "driver/i2s_std.h" // 引入 I2S 标准驱动
#include "esp_system.h" // 引入 ESP32 系统相关头文件
#include "esp_check.h" // 引入错误检查功能
#include "es8311.h" // 引入 ES8311 音频编解码器驱动
#include "esp32_s3_szp.h" // 引入 ESP32 S3 特定的硬件配置头文件


static const char *TAG = "i2s_es8311";  // 日志标签，用于调试信息输出
static const char err_reason[][30] = {"input param is invalid",  // 错误原因数组
                                      "operation timeout"
                                     };
static i2s_chan_handle_t tx_handle = NULL; // 定义 I2S 发送通道句柄


/* Import music file as buffer */
extern const uint8_t music_pcm_start[] asm("_binary_canon_pcm_start"); // PCM 音乐文件起始地址
extern const uint8_t music_pcm_end[]   asm("_binary_canon_pcm_end");// PCM 音乐文件结束地址

// 初始化 I2C 接口并初始化 ES8311 芯片
static esp_err_t es8311_codec_init(void)
{
    /* 初始化I2C接口 */
    ESP_ERROR_CHECK(bsp_i2c_init()); // 检查 I2C 初始化是否成功

    /* 初始化ES8311芯片 */
    es8311_handle_t es_handle = es8311_create(BSP_I2C_NUM, ES8311_ADDRRES_0); // 创建 ES8311 句柄
    ESP_RETURN_ON_FALSE(es_handle, ESP_FAIL, TAG, "es8311 create failed"); // 检查句柄是否有效

    // 配置 ES8311 时钟参数
    const es8311_clock_config_t es_clk = {
        .mclk_inverted = false, // 主时钟是否反转
        .sclk_inverted = false,// 串行时钟是否反转
        .mclk_from_mclk_pin = true, // 主时钟是否来自 MCLK 引脚
        .mclk_frequency = EXAMPLE_MCLK_FREQ_HZ, // 主时钟频率
        .sample_frequency = EXAMPLE_SAMPLE_RATE // 采样频率
    };

    // 初始化 ES8311 芯片
    ESP_ERROR_CHECK(es8311_init(es_handle, &es_clk, ES8311_RESOLUTION_16, ES8311_RESOLUTION_16));// 检查初始化是否成功
     // 设置采样频率
    ESP_RETURN_ON_ERROR(es8311_sample_frequency_config(es_handle, EXAMPLE_SAMPLE_RATE * EXAMPLE_MCLK_MULTIPLE, EXAMPLE_SAMPLE_RATE), TAG, "set es8311 sample frequency failed");
    ESP_RETURN_ON_ERROR(es8311_voice_volume_set(es_handle, EXAMPLE_VOICE_VOLUME, NULL), TAG, "set es8311 volume failed");// 设置音量
    ESP_RETURN_ON_ERROR(es8311_microphone_config(es_handle, false), TAG, "set es8311 microphone failed");// 配置麦克风

    return ESP_OK;// 返回成功   
}

// 初始化I2S外设
static esp_err_t i2s_driver_init(void)
{
    /* 配置 I2S 发送通道 */
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM, I2S_ROLE_MASTER);  // 创建 I2S 通道配置
    chan_cfg.auto_clear = true; // 自动清除 DMA 缓冲区中的旧数据
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &tx_handle, NULL)); // 创建新的 I2S 通道


    /* 初始化 I2S 为标准模式并打开 I2S 发送通道 */
    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(EXAMPLE_SAMPLE_RATE), // 时钟配置
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO), // 槽配置
        .gpio_cfg = { // GPIO 配置
            .mclk = I2S_MCK_IO, // 主时钟引脚
            .bclk = I2S_BCK_IO, // 位时钟引脚
            .ws = I2S_WS_IO, // 字选择引脚
            .dout = I2S_DO_IO,// 数据输出引脚
            .din = I2S_DI_IO, // 数据输入引脚
            .invert_flags = { // 反转标志配置
                .mclk_inv = false, // 主时钟反转标志
                .bclk_inv = false, // 位时钟反转标志
                .ws_inv = false, // 字选择反转标志
            },
        },
    };
    std_cfg.clk_cfg.mclk_multiple = EXAMPLE_MCLK_MULTIPLE; // 设置主时钟倍数

    ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_handle, &std_cfg)); // 初始化 I2S 通道为标准模式
    ESP_ERROR_CHECK(i2s_channel_enable(tx_handle)); // 启用 I2S 通道

    return ESP_OK; // 返回成功
}

// 播放音乐的任务
static void i2s_music(void *args)
{
    esp_err_t ret = ESP_OK; // 初始化返回值
    size_t bytes_write = 0; // 已写入字节数
    uint8_t *data_ptr = (uint8_t *)music_pcm_start; // 数据指针指向音乐 PCM 文件的起始地址

    /* （可选）在启用 TX 通道之前禁用 TX 通道并预加载数据，
     * 这样有效数据可以立即传输 */
    ESP_ERROR_CHECK(i2s_channel_disable(tx_handle));  // 禁用 I2S 发送通道
    ESP_ERROR_CHECK(i2s_channel_preload_data(tx_handle, data_ptr, music_pcm_end - data_ptr, &bytes_write)); // 预加载数据
    data_ptr += bytes_write; // 移动数据指针

    /* 启用 TX 通道*/
    ESP_ERROR_CHECK(i2s_channel_enable(tx_handle)); // 启用 I2S 发送通道
    while (1) {
         /* 将音乐写入耳机 */
        ret = i2s_channel_write(tx_handle, data_ptr, music_pcm_end - data_ptr, &bytes_write, portMAX_DELAY);  // 写入数据到 I2S 通道
        if (ret != ESP_OK) {// 检查写入操作是否成功
            /* 由于我们在 'i2s_channel_write' 中将超时设置为 'portMAX_DELAY'
               所以除非您设置其他超时值，否则不会到达这里，
               如果检测到超时，意味着写入操作失败。 */
            ESP_LOGE(TAG, "[music] i2s write failed, %s", err_reason[ret == ESP_ERR_TIMEOUT]); // 打印错误信息
            abort(); // 中止程序
        }
        if (bytes_write > 0) { // 检查写入的字节数是否大于零
            ESP_LOGI(TAG, "[music] i2s music played, %d bytes are written.", bytes_write); // 打印写入的字节数
        } else {
            ESP_LOGE(TAG, "[music] i2s music play failed."); // 打印播放失败信息
            abort(); // 中止程序
        }
        data_ptr = (uint8_t *)music_pcm_start; // 重置数据指针到起始位置
        vTaskDelay(1000 / portTICK_PERIOD_MS); // 延迟 1 秒
    }
    vTaskDelete(NULL); // 删除任务
}


// 应用主函数
void app_main(void)
{
    printf("i2s es8311 codec example start\n-----------------------------\n"); // 打印启动信息
    /* 初始化I2S外设 */
    if (i2s_driver_init() != ESP_OK) { // 初始化 I2S 驱动并检查是否成功
        ESP_LOGE(TAG, "i2s driver init failed");// 打印初始化失败信息
        abort();// 中止程序
    } else {
        ESP_LOGI(TAG, "i2s driver init success"); // 打印初始化成功信息
    }
    /* 初始化 I2C 以及初始化 ES8311 芯片 */
    if (es8311_codec_init() != ESP_OK) { // 初始化 ES8311 编解码器并检查是否成功
        ESP_LOGE(TAG, "es8311 codec init failed");// 打印初始化失败信息
        abort();// 中止程序
    } else {
        ESP_LOGI(TAG, "es8311 codec init success");// 打印初始化成功信息
    }

    pca9557_init(); //初始化 IO 扩展芯片
    pa_en(1); // 打开音频
    /* 创建播放音乐任务 */
    xTaskCreate(i2s_music, "i2s_music", 4096, NULL, 5, NULL); // 创建播放音乐的任务
}
