#include <stdio.h>  // 包含标准输入输出库
#include "esp32_s3_szp.h"  // 包含 ESP32-S3 SZP 相关的头文件

static const char *TAG = "esp32_s3_szp";  // 定义日志标签，用于调试和日志输出

/******************************************************************************/
/***************************  I2C ↓ *******************************************/
// 初始化 I2C 接口
esp_err_t bsp_i2c_init(void)
{
    i2c_config_t i2c_conf = {// 定义 I2C 配置结构体
        .mode = I2C_MODE_MASTER,// 设置 I2C 模式为主模式
        .sda_io_num = BSP_I2C_SDA,// 设置 SDA 引脚
        .sda_pullup_en = GPIO_PULLUP_ENABLE,// 启用 SDA 上拉
        .scl_io_num = BSP_I2C_SCL,// 设置 SCL 引脚
        .scl_pullup_en = GPIO_PULLUP_ENABLE,// 启用 SCL 上拉
        .master.clk_speed = BSP_I2C_FREQ_HZ// 设置 I2C 时钟频率
    };
    i2c_param_config(BSP_I2C_NUM, &i2c_conf);// 配置 I2C 参数

    return i2c_driver_install(BSP_I2C_NUM, i2c_conf.mode, 0, 0, 0);// 安装 I2C 驱动
}
/***************************  I2C ↑  *******************************************/
/*******************************************************************************/


/***********************************************************/
/***************    IO扩展芯片 ↓   *************************/

// 读取 PCA9557 寄存器的值
esp_err_t pca9557_register_read(uint8_t reg_addr, uint8_t *data, size_t len)
{
    return i2c_master_write_read_device(BSP_I2C_NUM, PCA9557_SENSOR_ADDR,  &reg_addr, 1, data, len, 1000 / portTICK_PERIOD_MS);// 通过 I2C 读取 PCA9557 的寄存器值 
}

// 给 PCA9557 的寄存器写值
esp_err_t pca9557_register_write_byte(uint8_t reg_addr, uint8_t data)
{
    uint8_t write_buf[2] = {reg_addr, data};// 创建写入缓冲区，包含寄存器地址和数据

    return i2c_master_write_to_device(BSP_I2C_NUM, PCA9557_SENSOR_ADDR, write_buf, sizeof(write_buf), 1000 / portTICK_PERIOD_MS);// 通过 I2C 写入数据到 PCA9557 的寄存器
}

// 初始化 PCA9557 IO 扩展芯片
void pca9557_init(void)
{
    // 写入控制引脚默认值 DVP_PWDN=1  PA_EN = 0  LCD_CS = 1
    pca9557_register_write_byte(PCA9557_OUTPUT_PORT, 0x05);  
    // 把 PCA9557 芯片的 IO1、IO2 设置为输出，其他引脚保持默认输入
    pca9557_register_write_byte(PCA9557_CONFIGURATION_PORT, 0xf8); 
}

// 设置 PCA9557 芯片的某个 IO 引脚输出高低电平
esp_err_t pca9557_set_output_state(uint8_t gpio_bit, uint8_t level)
{
    uint8_t data;// 存储读取到的数据
    esp_err_t res = ESP_FAIL;// 初始化返回值

    pca9557_register_read(PCA9557_OUTPUT_PORT, &data, 1);// 读取当前输出状态
    res = pca9557_register_write_byte(PCA9557_OUTPUT_PORT, SET_BITS(data, gpio_bit, level)); // 更新状态并写入

    return res;// 返回操作结果
}

// 控制 PCA9557_LCD_CS 引脚输出高低电平 参数0输出低电平 参数1输出高电平 
void lcd_cs(uint8_t level)
{
    pca9557_set_output_state(LCD_CS_GPIO, level);// 设置 LCD_CS 引脚状态
}

// 控制 PCA9557_PA_EN 引脚输出高低电平 参数0输出低电平 参数1输出高电平 
void pa_en(uint8_t level)
{
    pca9557_set_output_state(PA_EN_GPIO, level);// 设置 PA_EN 引脚状态
}

// 控制 PCA9557_DVP_PWDN 引脚输出高低电平 参数0输出低电平 参数1输出高电平 
void dvp_pwdn(uint8_t level)
{
    pca9557_set_output_state(DVP_PWDN_GPIO, level);// 设置 DVP_PWDN 引脚状态
}

/***************    IO扩展芯片 ↑   *************************/
/***********************************************************/


/***********************************************************/
/****************    LCD显示屏 ↓   *************************/

// 背光 PWM 初始化
esp_err_t bsp_display_brightness_init(void)
{
    //  设置 LEDC 外设用于 PWM 背光控制
    const ledc_channel_config_t LCD_backlight_channel = {
        .gpio_num = BSP_LCD_BACKLIGHT,// 背光引脚
        .speed_mode = LEDC_LOW_SPEED_MODE,// 低速模式
        .channel = LCD_LEDC_CH,// LEDC 通道
        .intr_type = LEDC_INTR_DISABLE,// 禁用中断
        .timer_sel = 0, // 选择定时器
        .duty = 0,// 初始占空比
        .hpoint = 0,// 高点
        .flags.output_invert = true// 输出反转
    };
    const ledc_timer_config_t LCD_backlight_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,// 低速模式
        .duty_resolution = LEDC_TIMER_10_BIT,// 占空比分辨率为 10 位
        .timer_num = 0,// 定时器编号
        .freq_hz = 5000,// PWM 频率
        .clk_cfg = LEDC_AUTO_CLK// 自动时钟配置
    };

    ESP_ERROR_CHECK(ledc_timer_config(&LCD_backlight_timer));// 配置定时器
    ESP_ERROR_CHECK(ledc_channel_config(&LCD_backlight_channel));// 配置 LEDC 通道

    return ESP_OK;// 返回成功
}

// 背光亮度设置
esp_err_t bsp_display_brightness_set(int brightness_percent)
{
    if (brightness_percent > 100) {// 限制最大亮度
        brightness_percent = 100;
    } else if (brightness_percent < 0) { // 限制最小亮度
        brightness_percent = 0;
    }

    ESP_LOGI(TAG, "Setting LCD backlight: %d%%", brightness_percent);// 日志输出设置的亮度
    // LEDC 分辨率设置为 10 位，因此：100% = 1023
    uint32_t duty_cycle = (1023 * brightness_percent) / 100; // 计算占空比
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, LCD_LEDC_CH, duty_cycle)); // 设置占空比
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, LCD_LEDC_CH));// 更新占空比

    return ESP_OK; // 返回成功
}

// 关闭背光
esp_err_t bsp_display_backlight_off(void)
{
    return bsp_display_brightness_set(0);// 设置亮度为 0 以关闭背光
}

// 打开背光，设置为最亮
esp_err_t bsp_display_backlight_on(void)
{
    return bsp_display_brightness_set(100);// 设置亮度为 100% 以打开背光
}

// 定义液晶屏句柄
static esp_lcd_panel_handle_t panel_handle = NULL;// 液晶屏面板句柄
esp_lcd_panel_io_handle_t io_handle = NULL; // 液晶屏 IO 句柄
static esp_lcd_touch_handle_t tp;   // 触摸屏句柄
static lv_disp_t *disp;      // 指向液晶屏
static lv_indev_t *disp_indev = NULL; // 指向触摸屏

// 液晶屏初始化
esp_err_t bsp_display_new(void)
{
    esp_err_t ret = ESP_OK;// 初始化返回值
    // 背光初始化
    ESP_RETURN_ON_ERROR(bsp_display_brightness_init(), TAG, "Brightness init failed");// 初始化背光
    // 初始化 SPI 总线
    ESP_LOGD(TAG, "Initialize SPI bus");// 日志输出初始化 SPI 总线
    const spi_bus_config_t buscfg = {
        .sclk_io_num = BSP_LCD_SPI_CLK, // SPI 时钟引脚
        .mosi_io_num = BSP_LCD_SPI_MOSI, // SPI 数据引脚
        .miso_io_num = GPIO_NUM_NC,// 未使用引脚
        .quadwp_io_num = GPIO_NUM_NC,// 未使用引脚
        .quadhd_io_num = GPIO_NUM_NC, // 未使用引脚
        .max_transfer_sz = BSP_LCD_H_RES * BSP_LCD_V_RES * sizeof(uint16_t),// 最大传输大小
    };
    ESP_RETURN_ON_ERROR(spi_bus_initialize(BSP_LCD_SPI_NUM, &buscfg, SPI_DMA_CH_AUTO), TAG, "SPI init failed");// 初始化 SPI 总线
    // 晶屏控制 IO 初始化
    ESP_LOGD(TAG, "Install panel IO");// 日志输出安装面板 IO
    const esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = BSP_LCD_DC,// 数据/命令引脚
        .cs_gpio_num = BSP_LCD_SPI_CS,// 片选引脚
        .pclk_hz = BSP_LCD_PIXEL_CLOCK_HZ,// 像素时钟频率
        .lcd_cmd_bits = LCD_CMD_BITS, // LCD 命令位数
        .lcd_param_bits = LCD_PARAM_BITS,// LCD 参数位数
        .spi_mode = 2,// SPI 模式
        .trans_queue_depth = 10,// 传输队列深度
    };
    ESP_GOTO_ON_ERROR(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)BSP_LCD_SPI_NUM, &io_config, &io_handle), err, TAG, "New panel IO failed");// 创建新的面板 IO
    // 初始化液晶屏驱动芯片ST7789
    ESP_LOGD(TAG, "Install LCD driver");// 日志输出安装 LCD 驱动
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = BSP_LCD_RST,// 复位引脚
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,// RGB 元素顺序
        .bits_per_pixel = BSP_LCD_BITS_PER_PIXEL,// 每像素位数
    };
    ESP_GOTO_ON_ERROR(esp_lcd_new_panel_st7789(io_handle, &panel_config, &panel_handle), err, TAG, "New panel failed");// 创建新的面板
    
    esp_lcd_panel_reset(panel_handle);  // 液晶屏复位
    lcd_cs(0); // 拉低 CS 引脚
    esp_lcd_panel_init(panel_handle);  // 初始化配置寄存器
    esp_lcd_panel_invert_color(panel_handle, true); // 颜色反转
    esp_lcd_panel_swap_xy(panel_handle, true);  // 显示翻转 
    esp_lcd_panel_mirror(panel_handle, true, false); // 镜像显示

    return ret;// 返回成功

err:
    if (panel_handle) {
        esp_lcd_panel_del(panel_handle);// 删除面板
    }
    if (io_handle) {
        esp_lcd_panel_io_del(io_handle);// 删除 IO 句柄
    }
    spi_bus_free(BSP_LCD_SPI_NUM);// 释放 SPI 总线
    return ret;// 返回错误
}

// LCD 显示初始化
esp_err_t bsp_lcd_init(void)
{
    esp_err_t ret = ESP_OK;// 初始化返回值

    
    ret = bsp_display_new(); // 液晶屏驱动初始化
    lcd_set_color(0x0000); // 设置整屏背景为黑色
    ret = esp_lcd_panel_disp_on_off(panel_handle, true); // 打开液晶屏显示
    ret = bsp_display_backlight_on(); // 打开背光显示

    return  ret;// 返回成功
}

// 液晶屏初始化并添加 LVGL 接口
static lv_disp_t *bsp_display_lcd_init(void)
{
    /* 初始化液晶屏 */
    bsp_display_new(); // 液晶屏驱动初始化
    lcd_set_color(0xffff); // 设置整屏背景白色
    esp_lcd_panel_disp_on_off(panel_handle, true); // 打开液晶屏显示

    /* 液晶屏添加LVGL接口 */
    ESP_LOGD(TAG, "Add LCD screen");// 日志输出添加 LCD 屏幕
    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = io_handle,// IO 句柄
        .panel_handle = panel_handle, // 面板句柄
        .buffer_size = BSP_LCD_H_RES * BSP_LCD_DRAW_BUF_HEIGHT,   // LVGL缓存大小 
        .double_buffer = false, // 是否开启双缓存
        .hres = BSP_LCD_H_RES, // 液晶屏的宽
        .vres = BSP_LCD_V_RES, // 液晶屏的高
        .monochrome = false,  // 是否单色显示器
        /* Rotation的值必须和液晶屏初始化里面设置的 翻转 和 镜像 一样 */
        .rotation = {
            .swap_xy = true,  // 是否翻转
            .mirror_x = true, // x方向是否镜像
            .mirror_y = false, // y方向是否镜像
        },
        .flags = {
            .buff_dma = false,  // 是否使用DMA 注意：dma与spiram不能同时为true
            .buff_spiram = true, // 是否使用PSRAM 注意：dma与spiram不能同时为true
        }
    };

    return lvgl_port_add_disp(&disp_cfg); // 添加 LVGL 显示接口
}

// 触摸屏初始化
esp_err_t bsp_touch_new(esp_lcd_touch_handle_t *ret_touch)
{
    /* 初始化触摸 */
    esp_lcd_touch_config_t tp_cfg = {
        .x_max = BSP_LCD_V_RES,// x 轴最大值
        .y_max = BSP_LCD_H_RES,// y 轴最大值
        .rst_gpio_num = GPIO_NUM_NC, // 复位引脚（与 LCD 复位共享）
        .int_gpio_num = GPIO_NUM_NC, // 中断引脚
        .levels = {
            .reset = 0,// 复位电平
            .interrupt = 0,// 中断电平
        },
        .flags = {
            .swap_xy = 1,// 是否交换 x 和 y
            .mirror_x = 1,// x 方向是否镜像
            .mirror_y = 0,// y 方向是否镜像
        },
    };
    esp_lcd_panel_io_handle_t tp_io_handle = NULL;// 触摸 IO 句柄
    esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_FT5x06_CONFIG(); // 触摸 IO I2C 配置

    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_i2c((esp_lcd_i2c_bus_handle_t)BSP_I2C_NUM, &tp_io_config, &tp_io_handle), TAG, "");// 创建触摸 IO
    ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_ft5x06(tp_io_handle, &tp_cfg, ret_touch));// 创建 FT5x06 触摸屏

    return ESP_OK; // 返回成功 
}

// 触摸屏初始化+添加LVGL接口
static lv_indev_t *bsp_display_indev_init(lv_disp_t *disp)
{
    /* 初始化触摸屏 */
    ESP_ERROR_CHECK(bsp_touch_new(&tp));// 初始化触摸屏
    assert(tp);// 确保触摸屏句柄有效

    /* 添加LVGL接口 */
    const lvgl_port_touch_cfg_t touch_cfg = {
        .disp = disp,// 液晶屏句柄
        .handle = tp,// 触摸屏句柄
    };

    return lvgl_port_add_touch(&touch_cfg); // 添加 LVGL 触摸接口
}

// 开发板显示初始化
void bsp_lvgl_start(void)
{
    /* 初始化LVGL */
    lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG(); // LVGL 配置
    lvgl_port_init(&lvgl_cfg);// 初始化 LVGL

    /* 初始化液晶屏 并添加LVGL接口 */
    disp = bsp_display_lcd_init();// 初始化液晶屏

    /* 初始化触摸屏 并添加LVGL接口 */
    disp_indev = bsp_display_indev_init(disp);// 初始化触摸屏

    /* 打开液晶屏背光 */
    bsp_display_backlight_on();// 打开背光

}


// 设置液晶屏颜色
void lcd_set_color(uint16_t color)
{
    // 分配内存 这里分配了液晶屏一行数据需要的大小
    uint16_t *buffer = (uint16_t *)heap_caps_malloc(BSP_LCD_H_RES * sizeof(uint16_t), MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);
    
    if (NULL == buffer)
    {
        ESP_LOGE(TAG, "Memory for bitmap is not enough");
    }
    else
    {
        for (size_t i = 0; i < BSP_LCD_H_RES; i++) // 给缓存中放入颜色数据
        {
            buffer[i] = color;// 设置颜色
        }
        for (int y = 0; y < 240; y++) // 显示整屏颜色
        {
            esp_lcd_panel_draw_bitmap(panel_handle, 0, y, 320, y+1, buffer);// 绘制一行颜色
        }
        free(buffer); // 释放内存
    }
}
/***************    LCD显示屏 ↑   *************************/
/***********************************************************/




/***********************************************************/
/***************    SPIFFS文件系统 ↓   *********************/

//挂载 SPIFFS 文件系统
esp_err_t bsp_spiffs_mount(void)
{
    esp_vfs_spiffs_conf_t conf = {
        .base_path = SPIFFS_BASE,// SPIFFS 基础路径
        .partition_label = "storage",// 分区标签
        .max_files = 5, // 最大文件数
        .format_if_mount_failed = false,// 如果挂载失败是否格式化   
    };

    esp_err_t ret_val = esp_vfs_spiffs_register(&conf); // 注册 SPIFFS 文件系统

    ESP_ERROR_CHECK(ret_val);// 检查返回值

    size_t total = 0, used = 0; // 定义总大小和已用大小
    ret_val = esp_spiffs_info(conf.partition_label, &total, &used);// 获取 SPIFFS 分区信息
    if (ret_val != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret_val));// 日志输出获取分区信息失败
    } else {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);// 日志输出分区大小
    }

    return ret_val;// 返回结果
}

/***************    SPIFFS文件系统 ↑  *********************/
/**********************************************************/



/***********************************************************/
/*********************    音频 ↓   *************************/
static esp_codec_dev_handle_t play_dev_handle;    // speaker句柄
static esp_codec_dev_handle_t record_dev_handle;  // microphone句柄

static i2s_chan_handle_t i2s_tx_chan = NULL; // 发送通道
static i2s_chan_handle_t i2s_rx_chan = NULL; // 接收通道
static const audio_codec_data_if_t *i2s_data_if = NULL;  /* Codec 数据接口 */


// I2S总线初始化
esp_err_t bsp_audio_init(void)
{
    esp_err_t ret = ESP_FAIL;// 初始化返回值
    if (i2s_tx_chan && i2s_rx_chan) {// 检查音频是否已初始化
        /* 如果已初始化，返回成功 */
        return ESP_OK;
    }

    /* 设置 I2S 外设 */
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(BSP_I2S_NUM, I2S_ROLE_MASTER);// 创建 I2S 通道配置
    chan_cfg.auto_clear = true; // 自动清除 DMA 缓冲区中的旧数据
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &i2s_tx_chan, &i2s_rx_chan));// 创建新的 I2S 通道

    /* 设置 I2S 通道 */
    const i2s_std_config_t std_cfg_default = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(16000),   // 设置采样率为 16000
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(32, I2S_SLOT_MODE_STEREO),  // 32 位 立体声配置
        .gpio_cfg = { 
            .mclk = GPIO_I2S_MCLK, // MCLK 引脚
            .bclk = GPIO_I2S_SCLK, // BCLK 引脚
            .ws   = GPIO_I2S_LRCK,  // WS 引脚
            .dout = GPIO_I2S_DOUT, // 数据输出引脚
            .din  = GPIO_I2S_SDIN,// 数据输入引脚
        },
    };

    if (i2s_tx_chan != NULL) {// 如果发送通道不为空
        ESP_GOTO_ON_ERROR(i2s_channel_init_std_mode(i2s_tx_chan, &std_cfg_default), err, TAG, "I2S channel initialization failed");// 初始化发送通道
        ESP_GOTO_ON_ERROR(i2s_channel_enable(i2s_tx_chan), err, TAG, "I2S enabling failed");// 启用发送通道
    }
    if (i2s_rx_chan != NULL) { // 如果接收通道不为空
        ESP_GOTO_ON_ERROR(i2s_channel_init_std_mode(i2s_rx_chan, &std_cfg_default), err, TAG, "I2S channel initialization failed");// 初始化接收通道
        ESP_GOTO_ON_ERROR(i2s_channel_enable(i2s_rx_chan), err, TAG, "I2S enabling failed");// 启用接收通道
    }

    audio_codec_i2s_cfg_t i2s_cfg = {// 创建 I2S 配置
        .port = BSP_I2S_NUM, // I2S 端口
        .rx_handle = i2s_rx_chan,// 接收通道句柄
        .tx_handle = i2s_tx_chan,// 发送通道句柄
    };
    i2s_data_if = audio_codec_new_i2s_data(&i2s_cfg);// 创建新的 I2S 数据接口
    if (i2s_data_if == NULL) {   // 检查 I2S 数据接口是否创建成功
        goto err;// 失败则跳转到错误处理
    }   

    return ESP_OK;// 返回成功

err:
    if (i2s_tx_chan) {
        i2s_del_channel(i2s_tx_chan);// 删除发送通道
    }
    if (i2s_rx_chan) {
        i2s_del_channel(i2s_rx_chan);// 删除接收通道
    }

    return ret;// 返回错误
}

// 初始化音频输出芯片
esp_codec_dev_handle_t bsp_audio_codec_speaker_init(void)
{
    if (i2s_data_if == NULL) {// 如果 I2S 数据接口为空
        /* 配置 I2S 外设和功率放大器 */
        ESP_ERROR_CHECK(bsp_audio_init());// 初始化音频
    }
    assert(i2s_data_if); // 确保 I2S 数据接口有效

    const audio_codec_gpio_if_t *gpio_if = audio_codec_new_gpio();// 创建 GPIO 接口

    audio_codec_i2c_cfg_t i2c_cfg = {// 创建 I2C 配置
        .port = BSP_I2C_NUM,// I2C 端口
        .addr = ES8311_CODEC_DEFAULT_ADDR,// 默认地址
    };
    const audio_codec_ctrl_if_t *i2c_ctrl_if = audio_codec_new_i2c_ctrl(&i2c_cfg);// 创建 I2C 控制接口
    assert(i2c_ctrl_if);// 确保 I2C 控制接口有效

    esp_codec_dev_hw_gain_t gain = {// 创建硬件增益配置
        .pa_voltage = 5.0,// 功率放大器电压
        .codec_dac_voltage = 3.3,// DAC 电压
    };

    es8311_codec_cfg_t es8311_cfg = {// 创建 ES8311 编解码器配置
        .ctrl_if = i2c_ctrl_if,// 控制接口
        .gpio_if = gpio_if,// GPIO 接口
        .codec_mode = ESP_CODEC_DEV_WORK_MODE_DAC,// 编解码器工作模式为 DAC
        .pa_pin = GPIO_PWR_CTRL, // 功率放大器引脚
        .pa_reverted = false,// 功率放大器是否反转
        .master_mode = false,// 是否为主模式
        .use_mclk = true,// 是否使用 MCLK
        .digital_mic = false, // 是否使用数字麦克风
        .invert_mclk = false,// MCLK 是否反转
        .invert_sclk = false,// SCLK 是否反转
        .hw_gain = gain, // 硬件增益配置
    };
    const audio_codec_if_t *es8311_dev = es8311_codec_new(&es8311_cfg);// 创建 ES8311 编解码器
    assert(es8311_dev);// 确保 ES8311 编解码器有效

    esp_codec_dev_cfg_t codec_dev_cfg = {// 创建编解码器设备配置
        .dev_type = ESP_CODEC_DEV_TYPE_OUT, // 设备类型为输出
        .codec_if = es8311_dev,// 编解码器接口
        .data_if = i2s_data_if, // 数据接口
    };
    return esp_codec_dev_new(&codec_dev_cfg);// 创建新的编解码器设备
}

// 初始化音频输入芯片
esp_codec_dev_handle_t bsp_audio_codec_microphone_init(void)
{
    if (i2s_data_if == NULL) {// 如果 I2S 数据接口为空
        /* 配置 I2S 外设和功率放大器 */
        ESP_ERROR_CHECK(bsp_audio_init()); // 初始化音频
    }
    assert(i2s_data_if);// 确保 I2S 数据接口有效

    audio_codec_i2c_cfg_t i2c_cfg = {// 创建 I2C 配置
        .port = BSP_I2C_NUM,// I2C 端口
        .addr = 0x82,// 默认地址
    };
    const audio_codec_ctrl_if_t *i2c_ctrl_if = audio_codec_new_i2c_ctrl(&i2c_cfg);// 创建 I2C 控制接口
    assert(i2c_ctrl_if);// 确保 I2C 控制接口有效

    es7210_codec_cfg_t es7210_cfg = {// 创建 ES7210 编解码器配置
        .ctrl_if = i2c_ctrl_if, // 控制接口
    };
    const audio_codec_if_t *es7210_dev = es7210_codec_new(&es7210_cfg);// 创建 ES7210 编解码器
    assert(es7210_dev);// 确保 ES7210 编解码器有效

    esp_codec_dev_cfg_t codec_es7210_dev_cfg = {// 创建 ES7210 编解码器设备配置
        .dev_type = ESP_CODEC_DEV_TYPE_IN,// 设备类型为输入
        .codec_if = es7210_dev, // 编解码器接口
        .data_if = i2s_data_if,// 数据接口
    };
    return esp_codec_dev_new(&codec_es7210_dev_cfg);// 创建新的编解码器设备
}

// 设置采样率
esp_err_t bsp_codec_set_fs(uint32_t rate, uint32_t bits_cfg, i2s_slot_mode_t ch)
{
    esp_err_t ret = ESP_OK;// 初始化返回值

    esp_codec_dev_sample_info_t fs = {// 创建采样信息结构体
        .sample_rate = rate,// 采样率
        .channel = ch,// 通道模式
        .bits_per_sample = bits_cfg,// 每个样本的位数
    };
    
    if (play_dev_handle) { // 如果播放器句柄有效
        ret = esp_codec_dev_close(play_dev_handle); // 关闭播放器设备
    }
    // if (record_dev_handle) {
    //     ret |= esp_codec_dev_close(record_dev_handle); // 关闭录音设备
    //     ret |= esp_codec_dev_set_in_gain(record_dev_handle, CODEC_DEFAULT_ADC_VOLUME);// 设置输入增益
    // }

    if (play_dev_handle) {// 如果播放器句柄有效
        ret |= esp_codec_dev_open(play_dev_handle, &fs);// 打开播放器设备
    }
    // if (record_dev_handle) {
    //     ret |= esp_codec_dev_open(record_dev_handle, &fs);// 打开录音设备
    // }
    return ret;// 返回结果
}

// 音频芯片初始化
esp_err_t bsp_codec_init(void)
{
    play_dev_handle = bsp_audio_codec_speaker_init();// 初始化音频输出设备
    assert((play_dev_handle) && "play_dev_handle not initialized");// 确保播放器句柄有效

    record_dev_handle = bsp_audio_codec_microphone_init();// 初始化音频输入设备
    assert((record_dev_handle) && "record_dev_handle not initialized");// 确保录音设备句柄有效

    bsp_codec_set_fs(CODEC_DEFAULT_SAMPLE_RATE, CODEC_DEFAULT_BIT_WIDTH, CODEC_DEFAULT_CHANNEL); // 设置采样率

    return ESP_OK;// 返回成功
}

// 播放音乐
esp_err_t bsp_i2s_write(void *audio_buffer, size_t len, size_t *bytes_written, uint32_t timeout_ms)
{
    esp_err_t ret = ESP_OK;// 初始化返回值
    ret = esp_codec_dev_write(play_dev_handle, audio_buffer, len);// 向播放器写入音频数据
    *bytes_written = len; // 更新已写入字节数
    return ret;// 返回结果
}

// 设置静音与否
esp_err_t bsp_codec_mute_set(bool enable)
{
    esp_err_t ret = ESP_OK;// 初始化返回值
    ret = esp_codec_dev_set_out_mute(play_dev_handle, enable);// 设置播放器静音状态
    return ret;// 返回结果
}

// 设置喇叭音量
esp_err_t bsp_codec_volume_set(int volume, int *volume_set)
{
    esp_err_t ret = ESP_OK;// 初始化返回值
    float v = volume;// 将音量转换为浮点数
    ret = esp_codec_dev_set_out_vol(play_dev_handle, (int)v); // 设置播放器音量
    return ret;// 返回结果
}

/*********************    音频 ↑   *************************/
/***********************************************************/


