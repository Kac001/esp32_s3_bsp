#include <stdio.h> // 引入标准输入输出库
#include "esp32_s3_szp.h"  // 引入 ESP32 S3 特定的硬件配置头文件


static const char *TAG = "esp32_s3_szp"; // 日志标签，用于调试信息输出

/******************************************************************************/
/***************************  I2C ↓ *******************************************/
// 初始化 I2C 接口
esp_err_t bsp_i2c_init(void)
{
    // 配置 I2C 参数
    i2c_config_t i2c_conf = {
        .mode = I2C_MODE_MASTER, // 设置为主模式
        .sda_io_num = BSP_I2C_SDA,// SDA 引脚
        .sda_pullup_en = GPIO_PULLUP_ENABLE,  // 启用 SDA 上拉电阻
        .scl_io_num = BSP_I2C_SCL, // SCL 引脚
        .scl_pullup_en = GPIO_PULLUP_ENABLE,// 启用 SCL 上拉电阻
        .master.clk_speed = BSP_I2C_FREQ_HZ // I2C 时钟频率
    };
    i2c_param_config(BSP_I2C_NUM, &i2c_conf); // 配置 I2C 参数

    return i2c_driver_install(BSP_I2C_NUM, i2c_conf.mode, 0, 0, 0);// 安装 I2C 驱动
}
/***************************  I2C ↑  *******************************************/
/*******************************************************************************/




/***********************************************************/
/***************    IO扩展芯片 ↓   *************************/

// 读取 PCA9557 寄存器的值
esp_err_t pca9557_register_read(uint8_t reg_addr, uint8_t *data, size_t len)
{
    // 使用 I2C 读取设备寄存器
    return i2c_master_write_read_device(BSP_I2C_NUM, PCA9557_SENSOR_ADDR,  &reg_addr, 1, data, len, 1000 / portTICK_PERIOD_MS);
}

// 给 PCA9557 的寄存器写值
esp_err_t pca9557_register_write_byte(uint8_t reg_addr, uint8_t data)
{
    uint8_t write_buf[2] = {reg_addr, data};//创建写入缓冲区
    // 使用 I2C 写入设备寄存
    return i2c_master_write_to_device(BSP_I2C_NUM, PCA9557_SENSOR_ADDR, write_buf, sizeof(write_buf), 1000 / portTICK_PERIOD_MS);
}

// 初始化 PCA9557 IO 扩展芯片
void pca9557_init(void)
{
    // 写入控制引脚默认值 DVP_PWDN=1  PA_EN = 0  LCD_CS = 1
    pca9557_register_write_byte(PCA9557_OUTPUT_PORT, 0x05);  
    // 把 PCA9557 芯片的 IO1 IO1 IO2 设置为输出，其他引脚保持默认的输入
    pca9557_register_write_byte(PCA9557_CONFIGURATION_PORT, 0xf8); 
}

//  设置 PCA9557 芯片的某个 IO 引脚输出高低电平
esp_err_t pca9557_set_output_state(uint8_t gpio_bit, uint8_t level)
{
    uint8_t data;// 存储寄存器数据
    esp_err_t res = ESP_FAIL;// 初始化返回值

    // 读取当前输出端口的状态
    pca9557_register_read(PCA9557_OUTPUT_PORT, &data, 1);
    // 设置指定引脚的状态并写回
    res = pca9557_register_write_byte(PCA9557_OUTPUT_PORT, SET_BITS(data, gpio_bit, level));

    return res;// 返回结果
}

// 控制 PCA9557_LCD_CS 引脚输出高低电平 参数0输出低电平 参数1输出高电平 
void lcd_cs(uint8_t level)
{
    pca9557_set_output_state(LCD_CS_GPIO, level); // 设置 LCD_CS 引脚状态
}

// 控制 PCA9557_PA_EN 引脚输出高低电平 参数0输出低电平 参数1输出高电平 
void pa_en(uint8_t level)
{
    pca9557_set_output_state(PA_EN_GPIO, level);// 设置 PA_EN 引脚状态
}

//控制 PCA9557_DVP_PWDN 引脚输出高低电平 参数0输出低电平 参数1输出高电平 
void dvp_pwdn(uint8_t level)
{
    pca9557_set_output_state(DVP_PWDN_GPIO, level);// 设置 DVP_PWDN 引脚状态
}

/***************    IO扩展芯片 ↑   *************************/
/***********************************************************/


/***********************************************************/
/****************    LCD显示屏 ↓   *************************/

// 背光PWM初始化
esp_err_t bsp_display_brightness_init(void)
{
    // 设置 LEDC 外设以控制 PWM 背光
    const ledc_channel_config_t LCD_backlight_channel = {
        .gpio_num = BSP_LCD_BACKLIGHT, // 背光引脚
        .speed_mode = LEDC_LOW_SPEED_MODE, // 低速模式
        .channel = LCD_LEDC_CH,// LEDC 通道
        .intr_type = LEDC_INTR_DISABLE,// 禁用中断
        .timer_sel = 0, // 选择定时器
        .duty = 0, // 初始占空比
        .hpoint = 0,// 高点
        .flags.output_invert = true // 输出反转
    };
    const ledc_timer_config_t LCD_backlight_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,// 低速模式
        .duty_resolution = LEDC_TIMER_10_BIT,// 10 位占空比分辨率
        .timer_num = 0,// 定时器编号
        .freq_hz = 5000,  // PWM 频率
        .clk_cfg = LEDC_AUTO_CLK // 自动选择时钟
    };

    ESP_ERROR_CHECK(ledc_timer_config(&LCD_backlight_timer));// 配置定时器
    ESP_ERROR_CHECK(ledc_channel_config(&LCD_backlight_channel));// 配置 LEDC 通道

    return ESP_OK;// 返回成功
}

// 背光亮度设置
esp_err_t bsp_display_brightness_set(int brightness_percent)
{   
     // 限制亮度值在 0 到 100 之间
    if (brightness_percent > 100) {
        brightness_percent = 100;
    } else if (brightness_percent < 0) {
        brightness_percent = 0;
    }

    ESP_LOGI(TAG, "Setting LCD backlight: %d%%", brightness_percent);// 打印设置的亮度值
    // LEDC 分辨率设置为 10 位，因此：100% = 1023
    uint32_t duty_cycle = (1023 * brightness_percent) / 100;  // 计算占空比
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, LCD_LEDC_CH, duty_cycle));// 设置占空比
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, LCD_LEDC_CH));// 更新占空比

    return ESP_OK; // 返回成功
}

// 关闭背光
esp_err_t bsp_display_backlight_off(void)
{
    return bsp_display_brightness_set(0);// 设置亮度为 0 以关闭背光
}

// 打开背光 设置为最亮
esp_err_t bsp_display_backlight_on(void)
{
    return bsp_display_brightness_set(100);// 设置亮度为 100% 以打开背光
}

// 定义液晶屏句柄
static esp_lcd_panel_handle_t panel_handle = NULL;// 液晶屏面板句柄
esp_lcd_panel_io_handle_t io_handle = NULL; // 液晶屏 IO 句柄

// 液晶屏初始化
esp_err_t bsp_display_new(void)
{
    esp_err_t ret = ESP_OK;// 初始化返回值
    // 背光初始化
    ESP_RETURN_ON_ERROR(bsp_display_brightness_init(), TAG, "Brightness init failed");// 检查背光初始化是否成功
    // 初始化SPI总线
    ESP_LOGD(TAG, "Initialize SPI bus");
    const spi_bus_config_t buscfg = {
        .sclk_io_num = BSP_LCD_SPI_CLK,// SPI 时钟引脚
        .mosi_io_num = BSP_LCD_SPI_MOSI,// SPI 数据输入引脚
        .miso_io_num = GPIO_NUM_NC,// 不使用 MISO 引脚
        .quadwp_io_num = GPIO_NUM_NC,// 不使用四线模式的写保护引脚
        .quadhd_io_num = GPIO_NUM_NC, // 不使用四线模式的高电平引脚
        .max_transfer_sz = BSP_LCD_H_RES * BSP_LCD_V_RES * sizeof(uint16_t), // 设置最大传输大小
    };
    ESP_RETURN_ON_ERROR(spi_bus_initialize(BSP_LCD_SPI_NUM, &buscfg, SPI_DMA_CH_AUTO), TAG, "SPI init failed");// 初始化 SPI 总线
    // 液晶屏控制 IO 初始化
    ESP_LOGD(TAG, "Install panel IO");
    const esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = BSP_LCD_DC,// 数据/命令引脚
        .cs_gpio_num = BSP_LCD_SPI_CS,// 片选引脚
        .pclk_hz = BSP_LCD_PIXEL_CLOCK_HZ, // 像素时钟频率
        .lcd_cmd_bits = LCD_CMD_BITS, // 命令位数
        .lcd_param_bits = LCD_PARAM_BITS,// 参数位数
        .spi_mode = 2,// SPI 模式
        .trans_queue_depth = 10,// 传输队列深度
    };
    // 创建新的面板 IO
    ESP_GOTO_ON_ERROR(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)BSP_LCD_SPI_NUM, &io_config, &io_handle), err, TAG, "New panel IO failed");
    // 初始化液晶屏驱动芯片 ST7789
    ESP_LOGD(TAG, "Install LCD driver");
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = BSP_LCD_RST,// 复位引脚
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,// RGB 元素顺序
        .bits_per_pixel = BSP_LCD_BITS_PER_PIXEL, // 每像素位数
    };
    ESP_GOTO_ON_ERROR(esp_lcd_new_panel_st7789(io_handle, &panel_config, &panel_handle), err, TAG, "New panel failed");// 创建新的液晶屏面板
    
    esp_lcd_panel_reset(panel_handle);   // 液晶屏复位
    lcd_cs(0);   // 拉低 CS 引脚
    esp_lcd_panel_init(panel_handle);  // 初始化配置寄存器
    esp_lcd_panel_invert_color(panel_handle, true); // 颜色反转
    esp_lcd_panel_swap_xy(panel_handle, true);  // 显示翻转 
    esp_lcd_panel_mirror(panel_handle, true, false); // 镜像

    return ret; // 返回成功

err:
    if (panel_handle) {
        esp_lcd_panel_del(panel_handle);// 删除面板句柄
    }
    if (io_handle) {
        esp_lcd_panel_io_del(io_handle);// 删除 IO 句柄
    }
    spi_bus_free(BSP_LCD_SPI_NUM);// 释放 SPI 总线
    return ret;// 返回错误
}

// LCD 显示初始化流程
esp_err_t bsp_lcd_init(void)
{
    esp_err_t ret = ESP_OK;// 初始化返回值

    
    ret = bsp_display_new(); // 液晶屏初始化
    lcd_set_color(0x0000); // 设置整屏背景为黑色
    ret = esp_lcd_panel_disp_on_off(panel_handle, true); // 打开液晶屏显示
    ret = bsp_display_backlight_on(); // 打开背光显示

    return  ret; // 返回结果
}

// 显示图片
void lcd_draw_pictrue(int x_start, int y_start, int x_end, int y_end, const unsigned char *gImage)
{
    // 分配内存，分配了需要的字节大小且指定在外部 SPIRAM 中分配
    size_t pixels_byte_size = (x_end - x_start)*(y_end - y_start) * 2;// 计算需要的字节大小
    uint16_t *pixels = (uint16_t *)heap_caps_malloc(pixels_byte_size, MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);// 在 SPIRAM 中分配内存
    if (NULL == pixels)// 检查内存分配是否成功
    {
        ESP_LOGE(TAG, "Memory for bitmap is not enough");// 打印错误信息
        return;// 返回
    }
    memcpy(pixels, gImage, pixels_byte_size);  // 把图片数据拷贝到内存
    esp_lcd_panel_draw_bitmap(panel_handle, x_start, y_start, x_end, y_end, (uint16_t *)pixels); // 显示整张图片数据
    heap_caps_free(pixels); // 释放内存
}

// 设置液晶屏颜色
void lcd_set_color(uint16_t color)
{
    // 分配内存 这里分配了液晶屏一行数据需要的大小
    uint16_t *buffer = (uint16_t *)heap_caps_malloc(BSP_LCD_H_RES * sizeof(uint16_t), MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);
    
    if (NULL == buffer)// 检查内存分配是否成功
    {
        ESP_LOGE(TAG, "Memory for bitmap is not enough");// 打印错误信息
    }
    else
    {
        for (size_t i = 0; i < BSP_LCD_H_RES; i++) // 给缓存中放入颜色数据
        {
            buffer[i] = color;// 设置颜色
        }
        for (int y = 0; y < 240; y++) // 显示整屏颜色
        {
            esp_lcd_panel_draw_bitmap(panel_handle, 0, y, 320, y+1, buffer);// 显示一行颜色
        }
        free(buffer); // 释放内存
    }
}
/***************    LCD显示屏 ↑   *************************/
/***********************************************************/



/***********************************************************/
/****************    摄像头 ↓   ****************************/

// 定义 LCD 显示队列句柄
static QueueHandle_t xQueueLCDFrame = NULL;

// 摄像头硬件初始化
void bsp_camera_init(void)
{
    dvp_pwdn(0); // 打开摄像头

    camera_config_t config;// 定义摄像头配置结构体
    config.ledc_channel = LEDC_CHANNEL_1;  // LEDC 通道选择，用于生成 XCLK 时钟（但 S3 不用）
    config.ledc_timer = LEDC_TIMER_1; // LEDC timer 选择，用于生成 XCLK 时钟（但 S3 不用）
    config.pin_d0 = CAMERA_PIN_D0;// 数据引脚配置
    config.pin_d1 = CAMERA_PIN_D1;// 数据引脚配置
    config.pin_d2 = CAMERA_PIN_D2; // 数据引脚配置
    config.pin_d3 = CAMERA_PIN_D3;// 数据引脚配置
    config.pin_d4 = CAMERA_PIN_D4;// 数据引脚配置
    config.pin_d5 = CAMERA_PIN_D5;// 数据引脚配置
    config.pin_d6 = CAMERA_PIN_D6;// 数据引脚配置
    config.pin_d7 = CAMERA_PIN_D7;// 数据引脚配置
    config.pin_xclk = CAMERA_PIN_XCLK;// XCLK 引脚配置
    config.pin_pclk = CAMERA_PIN_PCLK;// PCLK 引脚配置
    config.pin_vsync = CAMERA_PIN_VSYNC;// VSYNC 引脚配置
    config.pin_href = CAMERA_PIN_HREF;// HREF 引脚配置
    config.pin_sccb_sda = -1;   // 这里写-1 表示使用已经初始化的I2C接口
    config.pin_sccb_scl = CAMERA_PIN_SIOC;// SCCB SCL 引脚配置
    config.sccb_i2c_port = 0;// SCCB I2C 端口配置
    config.pin_pwdn = CAMERA_PIN_PWDN;// PWDN 引脚配置
    config.pin_reset = CAMERA_PIN_RESET;// RESET 引脚配置
    config.xclk_freq_hz = XCLK_FREQ_HZ;// XCLK 频率配置
    config.pixel_format = PIXFORMAT_RGB565;// 像素格式配置
    config.frame_size = FRAMESIZE_QVGA;// 帧大小配置
    config.jpeg_quality = 12;// JPEG 质量配置
    config.fb_count = 2;// 帧缓冲区数量配置
    config.fb_location = CAMERA_FB_IN_PSRAM; // 帧缓冲区位置配置
    config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;// 抓取模式配置

    // 摄像头初始化
    esp_err_t err = esp_camera_init(&config); // 配置上面定义的参数
    if (err != ESP_OK)// 检查初始化是否成功
    {
        ESP_LOGE(TAG, "Camera init failed with error 0x%x", err); // 打印初始化失败信息
        return;// 返回
    }

    sensor_t *s = esp_camera_sensor_get(); // 获取摄像头型号

    if (s->id.PID == GC0308_PID) { // 如果是 GC0308 摄像头
        s->set_hmirror(s, 1);  // 这里控制摄像头镜像 写1镜像 写0不镜像
    }
}

//  LCD 处理任务
static void task_process_lcd(void *arg)
{
    camera_fb_t *frame = NULL;// 定义摄像头帧指针

    while (true)// 无限循环
    {
        if (xQueueReceive(xQueueLCDFrame, &frame, portMAX_DELAY)) // 从队列接收帧
        {
            esp_lcd_panel_draw_bitmap(panel_handle, 0, 0, frame->width, frame->height, (uint16_t *)frame->buf); // 显示图像
            esp_camera_fb_return(frame);// 释放帧缓冲区
        }
    }
}

// 摄像头处理任务
static void task_process_camera(void *arg)
{
    while (true)// 无限循环
    {
        camera_fb_t *frame = esp_camera_fb_get();// 获取摄像头帧
        if (frame)// 如果帧获取成功
            xQueueSend(xQueueLCDFrame, &frame, portMAX_DELAY);// 将帧发送到 LCD 显示队列，在上面的task_process_lcd中的队列调用
    }
}

// 让摄像头显示到LCD
void app_camera_lcd(void)
{
    xQueueLCDFrame = xQueueCreate(2, sizeof(camera_fb_t *));// 创建 LCD 显示队列
    xTaskCreatePinnedToCore(task_process_camera, "task_process_camera", 3 * 1024, NULL, 5, NULL, 1); // 创建摄像头处理任务
    xTaskCreatePinnedToCore(task_process_lcd, "task_process_lcd", 4 * 1024, NULL, 5, NULL, 0); // 创建 LCD 处理任务
}

/********************    摄像头 ↑   *************************/
/***********************************************************/
