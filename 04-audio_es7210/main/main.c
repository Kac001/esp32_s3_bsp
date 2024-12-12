/*
 * SPDX-FileCopyrightText: 2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#include <string.h> // 引入字符串处理库
#include "sdkconfig.h" // 包含 SDK 配置
#include "esp_check.h" // 引入错误检查功能
#include "esp_vfs_fat.h" // 引入 FAT 文件系统支持
#include "driver/i2s_tdm.h" // 引入 I2S TDM 驱动
#include "driver/i2s_std.h" // 引入 I2S 标准驱动
#include "driver/i2c.h"  // 引入 I2C 驱动
#include "es7210.h" // 引入 ES7210 音频编解码器驱动
#include "format_wav.h" // 引入 WAV 文件格式处理
#include "sdmmc_cmd.h" // 引入 SD/MMC 命令支持
#include "driver/sdmmc_host.h" // 引入 SD/MMC 主机驱动

/* I2C 端口和 GPIO 引脚定义 */
#define EXAMPLE_I2C_NUM            (0) // I2C 端口编号
#define EXAMPLE_I2C_SDA_IO         (1) // I2C 数据引脚
#define EXAMPLE_I2C_SCL_IO         (2) // I2C 时钟引脚

/* I2S 端口和 GPIO 引脚定义 */
#define EXAMPLE_I2S_NUM            (0) // I2S 端口编号
#define EXAMPLE_I2S_MCK_IO         (38)// I2S 主时钟引脚
#define EXAMPLE_I2S_BCK_IO         (14)// I2S 位时钟引脚
#define EXAMPLE_I2S_WS_IO          (13)// I2S 字选择引脚
#define EXAMPLE_I2S_DI_IO          (12)// I2S 数据输入引脚

/* SD 卡 GPIO 引脚定义*/
#define EXAMPLE_SD_CMD_IO      (48) // SD 卡命令引脚
#define EXAMPLE_SD_CLK_IO      (47) // SD 卡时钟引脚
#define EXAMPLE_SD_DAT0_IO     (21) // SD 卡数据引脚 D0

/* I2S 配置参数 */
#define EXAMPLE_I2S_TDM_FORMAT     (ES7210_I2S_FMT_I2S) // I2S 格式
#define EXAMPLE_I2S_CHAN_NUM       (2) // 通道数
#define EXAMPLE_I2S_SAMPLE_RATE    (48000) // 采样率
#define EXAMPLE_I2S_MCLK_MULTIPLE  (I2S_MCLK_MULTIPLE_256) // 主时钟倍数
#define EXAMPLE_I2S_SAMPLE_BITS    (I2S_DATA_BIT_WIDTH_16BIT)// 采样位宽
#define EXAMPLE_I2S_TDM_SLOT_MASK  (I2S_TDM_SLOT0 | I2S_TDM_SLOT1)// TDM 槽掩码

/* ES7210 配置参数 */
#define EXAMPLE_ES7210_I2C_ADDR    (0x41) // ES7210 I2C 地址
#define EXAMPLE_ES7210_I2C_CLK     (100000) // I2C 时钟频率
#define EXAMPLE_ES7210_MIC_GAIN    (ES7210_MIC_GAIN_30DB) // 麦克风增益
#define EXAMPLE_ES7210_MIC_BIAS    (ES7210_MIC_BIAS_2V87) // 麦克风偏置电压
#define EXAMPLE_ES7210_ADC_VOLUME  (0) // ADC 音量

/* SD 卡与录音配置 */
#define EXAMPLE_RECORD_TIME_SEC    (10) // 录音时间（秒）
#define EXAMPLE_SD_MOUNT_POINT     "/sdcard" // SD 卡挂载点
#define EXAMPLE_RECORD_FILE_PATH   "/RECORD.WAV" // 录音文件路径

static const char *TAG = "example"; // 日志标签，用于调试信息输出

// 初始化 I2S 接收通道
static i2s_chan_handle_t es7210_i2s_init(void)
{
    i2s_chan_handle_t i2s_rx_chan = NULL;  // 定义接收通道句柄
    ESP_LOGI(TAG, "Create I2S receive channel");// 打印创建 I2S 接收通道信息
    i2s_chan_config_t i2s_rx_conf = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER); // 配置接收通道
    ESP_ERROR_CHECK(i2s_new_channel(&i2s_rx_conf, NULL, &i2s_rx_chan)); // 创建i2s通道
    ESP_LOGI(TAG, "Configure I2S receive channel to TDM mode"); // 打印配置 I2S 接收通道为 TDM 模式信息
    // 定义接收通道为 I2S TDM 模式并配置
    i2s_tdm_config_t i2s_tdm_rx_conf = {  
        .slot_cfg = I2S_TDM_PHILIPS_SLOT_DEFAULT_CONFIG(EXAMPLE_I2S_SAMPLE_BITS, I2S_SLOT_MODE_STEREO, EXAMPLE_I2S_TDM_SLOT_MASK), // 配置槽
        .clk_cfg  = { // 配置时钟
            .clk_src = I2S_CLK_SRC_DEFAULT, // 时钟源
            .sample_rate_hz = EXAMPLE_I2S_SAMPLE_RATE, // 采样率
            .mclk_multiple = EXAMPLE_I2S_MCLK_MULTIPLE // 主时钟倍数
        },
        .gpio_cfg = { // 配置 GPIO 引脚
            .mclk = EXAMPLE_I2S_MCK_IO,  // 主时钟引脚
            .bclk = EXAMPLE_I2S_BCK_IO, // 位时钟引脚
            .ws   = EXAMPLE_I2S_WS_IO, // 字选择引脚
            .dout = -1, // ES7210 仅有 ADC 功能，因此数据输出引脚无效
            .din  = EXAMPLE_I2S_DI_IO //数据输入引脚
        },
    };

    ESP_ERROR_CHECK(i2s_channel_init_tdm_mode(i2s_rx_chan, &i2s_tdm_rx_conf)); // 初始化 I2S 通道为 TDM 模式

    return i2s_rx_chan; // 返回接收通道句柄
}

// 挂载 SD 卡
sdmmc_card_t * mount_sdcard(void)
{
    sdmmc_card_t *sdmmc_card = NULL; // 定义 SD 卡指针

    ESP_LOGI(TAG, "Mounting SD card"); // 打印挂载 SD 卡信息
    esp_vfs_fat_sdmmc_mount_config_t mount_config = { // 配置挂载参数
        .format_if_mount_failed = true, // 如果挂载失败则格式化
        .max_files = 2,// 允许打开的最大文件数
        .allocation_unit_size = 8 * 1024 // 分配单元大小
    };

    ESP_LOGI(TAG, "Initializing SD card"); // 打印初始化 SD 卡信息
    ESP_LOGI(TAG, "Using SDMMC peripheral"); // 打印使用的 SDMMC 外设信息

    sdmmc_host_t sdmmc_host = SDMMC_HOST_DEFAULT(); // 获取默认的 SDMMC 主机配置
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT(); // 获取默认的 SDMMC 插槽配置
    slot_config.width = 1;  // 设置为 1 线 SD 模式
    slot_config.clk = EXAMPLE_SD_CLK_IO; // 设置时钟引脚
    slot_config.cmd = EXAMPLE_SD_CMD_IO;// 设置命令引脚
    slot_config.d0 = EXAMPLE_SD_DAT0_IO;// 设置数据引脚 D0
    slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;  // 启用内部上拉电阻

    ESP_LOGI(TAG, "Mounting filesystem"); // 打印挂载文件系统信息

    esp_err_t ret; // 定义返回值变量
    while (1) {// 循环尝试挂载 SD 卡
        ret = esp_vfs_fat_sdmmc_mount(EXAMPLE_SD_MOUNT_POINT, &sdmmc_host, &slot_config, &mount_config, &sdmmc_card); // 挂载 SD 卡
        if (ret == ESP_OK) { // 如果挂载成功
            break; // 跳出循环
        } else if (ret == ESP_FAIL) { // 如果挂载失败
            ESP_LOGE(TAG, "Failed to mount filesystem."); // 打印挂载失败信息
        } else { // 如果是其他错误
            ESP_LOGE(TAG, "Failed to initialize the card (%s). ", esp_err_to_name(ret)); // 打印错误名称
        }
        vTaskDelay(pdMS_TO_TICKS(1000));  // 延迟 1 秒后重试
    }

    // 打印 SD 卡信息
    ESP_LOGI(TAG, "Card size: %lluMB, speed: %dMHz",
            (((uint64_t)sdmmc_card->csd.capacity) * sdmmc_card->csd.sector_size) >> 20, // 计算卡容量（MB）
            sdmmc_card->max_freq_khz / 1000); // 打印卡的最大频率（MHz）

    return sdmmc_card; // 返回 SD 卡指针
}

// 初始化 ES7210 编解码器
static void es7210_codec_init(void)
{
    
    // 初始化 I2C 接口
    ESP_LOGI(TAG, "Init I2C used to configure ES7210"); // 打印初始化 I2C 信息
    i2c_config_t i2c_conf = { // 配置 I2C 参数
        .sda_io_num = EXAMPLE_I2C_SDA_IO, // SDA 引脚
        .scl_io_num = EXAMPLE_I2C_SCL_IO, // SCL 引脚
        .mode = I2C_MODE_MASTER, // 设置为主模式
        .sda_pullup_en = GPIO_PULLUP_ENABLE,  // 启用 SDA 上拉电阻
        .scl_pullup_en = GPIO_PULLUP_ENABLE, // 启用 SCL 上拉电阻
        .master.clk_speed = EXAMPLE_ES7210_I2C_CLK, // 设置 I2C 时钟频率
    };
    ESP_ERROR_CHECK(i2c_param_config(EXAMPLE_I2C_NUM, &i2c_conf)); // 配置 I2C 参数
    ESP_ERROR_CHECK(i2c_driver_install(EXAMPLE_I2C_NUM, i2c_conf.mode, 0, 0, 0)); // 安装 I2C 驱动

    // 创建 ES7210 设备句柄
    es7210_dev_handle_t es7210_handle = NULL; // 定义 ES7210 句柄
    es7210_i2c_config_t es7210_i2c_conf = { // 配置 ES7210 I2C 参数
        .i2c_port = EXAMPLE_I2C_NUM, // I2C 端口
        .i2c_addr = EXAMPLE_ES7210_I2C_ADDR // I2C 地址
    };
    ESP_ERROR_CHECK(es7210_new_codec(&es7210_i2c_conf, &es7210_handle));  // 创建 ES7210 编解码器

    // 初始化 ES7210 芯片
    ESP_LOGI(TAG, "Configure ES7210 codec parameters"); // 打印配置 ES7210 编解码器参数信息
    es7210_codec_config_t codec_conf = { // 配置编解码器参数
        .i2s_format = EXAMPLE_I2S_TDM_FORMAT, // I2S 格式
        .mclk_ratio = EXAMPLE_I2S_MCLK_MULTIPLE, // 主时钟倍数
        .sample_rate_hz = EXAMPLE_I2S_SAMPLE_RATE, // 采样率
        .bit_width = (es7210_i2s_bits_t)EXAMPLE_I2S_SAMPLE_BITS, // 采样位宽
        .mic_bias = EXAMPLE_ES7210_MIC_BIAS,  // 麦克风偏置电压
        .mic_gain = EXAMPLE_ES7210_MIC_GAIN, // 麦克风增益
        .flags.tdm_enable = true // 启用 TDM 模式
    };
    ESP_ERROR_CHECK(es7210_config_codec(es7210_handle, &codec_conf)); // 配置 ES7210 编解码器
    ESP_ERROR_CHECK(es7210_config_volume(es7210_handle, EXAMPLE_ES7210_ADC_VOLUME)); // 配置音量
} 

// 录音并保存为 WAV 文件
static esp_err_t record_wav(i2s_chan_handle_t i2s_rx_chan)
{
    ESP_RETURN_ON_FALSE(i2s_rx_chan, ESP_FAIL, TAG, "invalid i2s channel handle pointer"); // 检查 I2S 通道句柄有效性
    esp_err_t ret = ESP_OK; // 初始化返回值

    // 计算 WAV 文件的字节率和大小
    uint32_t byte_rate = EXAMPLE_I2S_SAMPLE_RATE * EXAMPLE_I2S_CHAN_NUM * EXAMPLE_I2S_SAMPLE_BITS / 8;  // 字节率
    uint32_t wav_size = byte_rate * EXAMPLE_RECORD_TIME_SEC; // WAV 文件大小

    // 创建 WAV 文件头
    const wav_header_t wav_header =
        WAV_HEADER_PCM_DEFAULT(wav_size, EXAMPLE_I2S_SAMPLE_BITS, EXAMPLE_I2S_SAMPLE_RATE, EXAMPLE_I2S_CHAN_NUM); // 默认 WAV 文件头

    ESP_LOGI(TAG, "Opening file %s", EXAMPLE_RECORD_FILE_PATH); // 打印打开文件信息
    FILE *f = fopen(EXAMPLE_SD_MOUNT_POINT EXAMPLE_RECORD_FILE_PATH, "w"); // 打开 WAV 文件进行写入
    ESP_RETURN_ON_FALSE(f, ESP_FAIL, TAG, "error while opening wav file"); // 检查文件打开是否成功

    /* 写入 WAV 文件头 */
    ESP_GOTO_ON_FALSE(fwrite(&wav_header, sizeof(wav_header_t), 1, f), ESP_FAIL, err, // 写入 WAV 文件头
                      TAG, "error while writing wav header");

    /* 开始录音 */
    size_t wav_written = 0; // 已写入的 WAV 数据大小
    static int16_t i2s_readraw_buff[4096];  // 定义缓冲区用于存储原始音频数据
    ESP_GOTO_ON_ERROR(i2s_channel_enable(i2s_rx_chan), err, TAG, "error while starting i2s rx channel"); // 启动 I2S 接收通道
    while (wav_written < wav_size) { // 循环直到写入 WAV 文件大小
        if(wav_written % byte_rate < sizeof(i2s_readraw_buff)) { // 每次写入的字节数小于缓冲区大小时打印录音进度
            ESP_LOGI(TAG, "Recording: %"PRIu32"/%ds", wav_written/byte_rate + 1, EXAMPLE_RECORD_TIME_SEC);// 打印录音进度
        }
        size_t bytes_read = 0; // 读取的字节数
        /* 从 ES7210 读取原始样本 */
        ESP_GOTO_ON_ERROR(i2s_channel_read(i2s_rx_chan, i2s_readraw_buff, sizeof(i2s_readraw_buff), &bytes_read, // 从 I2S 通道读取音频数据
                          pdMS_TO_TICKS(1000)), err, TAG, "error while reading samples from i2s");
        /* 将样本写入 WAV 文件 */
        ESP_GOTO_ON_FALSE(fwrite(i2s_readraw_buff, bytes_read, 1, f), ESP_FAIL, err, // 写入读取的音频数据到 WAV 文件
                          TAG, "error while writing samples to wav file");
        wav_written += bytes_read;  // 更新已写入的字节数
    }

err:
    i2s_channel_disable(i2s_rx_chan); // 停止 I2S 接收通道
    ESP_LOGI(TAG, "Recording done! Flushing file buffer");  // 打印录音完成信息
    fclose(f); // 关闭文件

    return ret; // 返回操作结果
}

// 应用主函数
void app_main(void)
{
    /*初始化 I2S 接口 */
    i2s_chan_handle_t i2s_rx_chan = es7210_i2s_init();  // 初始化 I2S 接收通道
    /* 初始化 ES7210 芯片 */
    es7210_codec_init(); // 初始化 ES7210 编解码器
    /* 挂载 SD 卡 */
    sdmmc_card_t *sdmmc_card = mount_sdcard(); // 挂载 SD 卡
    /* 录音 */
    esp_err_t err = record_wav(i2s_rx_chan); // 开始录音并保存为 WAV 文件
    /* 弹出SD卡 */
    esp_vfs_fat_sdcard_unmount(EXAMPLE_SD_MOUNT_POINT, sdmmc_card); // 卸载 SD 卡
    if(err == ESP_OK) { // 检查录音是否成功
        ESP_LOGI(TAG, "Audio was successfully recorded into "EXAMPLE_RECORD_FILE_PATH
                      ". You can now remove the SD card safely");  // 打印成功信息
    } else {
        ESP_LOGE(TAG, "Record failed, "EXAMPLE_RECORD_FILE_PATH" on SD card may not be playable."); // 打印失败信息
    }
}
