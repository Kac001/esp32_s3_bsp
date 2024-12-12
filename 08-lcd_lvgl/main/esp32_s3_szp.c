#include <stdio.h> // 引入标准输入输出库
#include "esp32_s3_szp.h" // 引入 ESP32 S3 特定的硬件配置头文件


static const char *TAG = "esp32_s3_szp"; // 日志标签，用于调试信息输出

/******************************************************************************/
/***************************  I2C ↓ *******************************************/
esp_err_t bsp_i2c_init(void)
{
    // 配置 I2C 参数
    i2c_config_t i2c_conf = {
        .mode = I2C_MODE_MASTER,// 设置为主模式
        .sda_io_num = BSP_I2C_SDA,// SDA 引脚
        .sda_pullup_en = GPIO_PULLUP_ENABLE, // 启用 SDA 上拉电阻
        .scl_io_num = BSP_I2C_SCL,// SCL 引脚
        .scl_pullup_en = GPIO_PULLUP_ENABLE,// 启用 SCL 上拉电阻
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
    // 使用 I2C 读取寄存器数据
    return i2c_master_write_read_device(BSP_I2C_NUM, PCA9557_SENSOR_ADDR,  &reg_addr, 1, data, len, 1000 / portTICK_PERIOD_MS);
}

// 给 PCA9557 的寄存器写值
esp_err_t pca9557_register_write_byte(uint8_t reg_addr, uint8_t data)
{
    uint8_t write_buf[2] = {reg_addr, data};// 创建写入缓冲区
    // 使用 I2C 写入寄存器数据  
    return i2c_master_write_to_device(BSP_I2C_NUM, PCA9557_SENSOR_ADDR, write_buf, sizeof(write_buf), 1000 / portTICK_PERIOD_MS);
}

// 初始化 PCA9557 IO 扩展芯片
void pca9557_init(void)
{
    // 写入控制引脚默认值 DVP_PWDN=1  PA_EN = 0  LCD_CS = 1
    pca9557_register_write_byte(PCA9557_OUTPUT_PORT, 0x05);  
    // 把 PCA9557 芯片的 IO1, IO2 设置为输出，其他引脚保持默认的输入
    pca9557_register_write_byte(PCA9557_CONFIGURATION_PORT, 0xf8); 
}

// 设置 PCA9557 芯片的某个 IO 引脚输出高低电平
esp_err_t pca9557_set_output_state(uint8_t gpio_bit, uint8_t level)
{
    uint8_t data; // 存储读取的寄存器值
    esp_err_t res = ESP_FAIL;// 初始化返回值

    // 读取当前输出端口的状态
    pca9557_register_read(PCA9557_OUTPUT_PORT, &data, 1);
    // 设置指定引脚的状态并写回
    res = pca9557_register_write_byte(PCA9557_OUTPUT_PORT, SET_BITS(data, gpio_bit, level));

    return res; // 返回结果
}

// 控控制 PCA9557_LCD_CS 引脚输出高低电平 参数0输出低电平 参数1输出高电平 
void lcd_cs(uint8_t level)
{
    pca9557_set_output_state(LCD_CS_GPIO, level); // 设置 LCD_CS 引脚状态
}

// 控制 PCA9557_PA_EN 引脚输出高低电平 参数0输出低电平 参数1输出高电平 
void pa_en(uint8_t level)
{
    pca9557_set_output_state(PA_EN_GPIO, level);// 设置 PA_EN 引脚状态
}

// 控制 PCA9557_DVP_PWDN 引脚输出高低电平 参数0输出低电平 参数1输出高电平
void dvp_pwdn(uint8_t level)
{
    pca9557_set_output_state(DVP_PWDN_GPIO, level); // 设置 DVP_PWDN 引脚状态
}

/***************    IO扩展芯片 ↑   *************************/
/***********************************************************/


/***********************************************************/
/****************    LCD显示屏 ↓   *************************/

// 背光 PWM 初始化
esp_err_t bsp_display_brightness_init(void)
{
    // 设置 LEDC 外设以控制 PWM 背光
    const ledc_channel_config_t LCD_backlight_channel = {
        .gpio_num = BSP_LCD_BACKLIGHT, // 背光引脚
        .speed_mode = LEDC_LOW_SPEED_MODE,// 低速模式
        .channel = LCD_LEDC_CH,// LEDC 通道
        .intr_type = LEDC_INTR_DISABLE,// 禁用中断
        .timer_sel = 0,// 选择定时器
        .duty = 0,// 初始占空比
        .hpoint = 0,// 高点
        .flags.output_invert = true// 输出反转
    };
    const ledc_timer_config_t LCD_backlight_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,// 低速模式
        .duty_resolution = LEDC_TIMER_10_BIT,// 10 位占空比分辨率
        .timer_num = 0,// 定时器编号
        .freq_hz = 5000,// PWM 频率
        .clk_cfg = LEDC_AUTO_CLK// 自动选择时钟
    };

    ESP_ERROR_CHECK(ledc_timer_config(&LCD_backlight_timer));// 配置定时器
    ESP_ERROR_CHECK(ledc_channel_config(&LCD_backlight_channel));// 配置 LEDC 通道

    return ESP_OK; // 返回成功
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
    uint32_t duty_cycle = (1023 * brightness_percent) / 100;// 计算占空比
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, LCD_LEDC_CH, duty_cycle));// 设置占空比
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, LCD_LEDC_CH)); // 更新占空比

    return ESP_OK;// 返回成功
}

// 关闭背光
esp_err_t bsp_display_backlight_off(void)
{
    return bsp_display_brightness_set(0); // 设置亮度为 0 以关闭背光
}

// 打开背光 最亮
esp_err_t bsp_display_backlight_on(void)
{
    return bsp_display_brightness_set(100);// 设置亮度为 100% 以打开背光
}

// 定义液晶屏句柄
static esp_lcd_panel_handle_t panel_handle = NULL; // 液晶屏面板句柄
esp_lcd_panel_io_handle_t io_handle = NULL; // 液晶屏 IO 句柄
static esp_lcd_touch_handle_t tp;   // 触摸屏句柄
static lv_disp_t *disp;      // 指向液晶屏
static lv_indev_t *disp_indev = NULL; // 指向触摸屏

// 液晶屏初始化
esp_err_t bsp_display_new(void)
{
    esp_err_t ret = ESP_OK; // 初始化返回值
    // 背光初始化
    ESP_RETURN_ON_ERROR(bsp_display_brightness_init(), TAG, "Brightness init failed"); // 检查背光初始化是否成功
    // 初始化SPI总线
    ESP_LOGD(TAG, "Initialize SPI bus");
    const spi_bus_config_t buscfg = {
        .sclk_io_num = BSP_LCD_SPI_CLK,// SPI 时钟引脚
        .mosi_io_num = BSP_LCD_SPI_MOSI,// SPI 数据输入引脚
        .miso_io_num = GPIO_NUM_NC, // 不使用 MISO 引脚
        .quadwp_io_num = GPIO_NUM_NC, // 不使用四线模式的写保护引脚
        .quadhd_io_num = GPIO_NUM_NC,// 不使用四线模式的高电平引脚
        .max_transfer_sz = BSP_LCD_H_RES * BSP_LCD_V_RES * sizeof(uint16_t),// 设置最大传输大小
    };
    ESP_RETURN_ON_ERROR(spi_bus_initialize(BSP_LCD_SPI_NUM, &buscfg, SPI_DMA_CH_AUTO), TAG, "SPI init failed");// 初始化 SPI 总线
    // 液晶屏控制IO初始化
    ESP_LOGD(TAG, "Install panel IO");
    const esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = BSP_LCD_DC,// 数据/命令引脚
        .cs_gpio_num = BSP_LCD_SPI_CS,// 片选引脚
        .pclk_hz = BSP_LCD_PIXEL_CLOCK_HZ,// 像素时钟频率
        .lcd_cmd_bits = LCD_CMD_BITS,// 命令位数
        .lcd_param_bits = LCD_PARAM_BITS,// 参数位数
        .spi_mode = 2,// SPI 模式
        .trans_queue_depth = 10,// 传输队列深度
    };
    ESP_GOTO_ON_ERROR(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)BSP_LCD_SPI_NUM, &io_config, &io_handle), err, TAG, "New panel IO failed");// 创建新的面板 IO
    // 初始化液晶屏驱动芯片 ST7789
    ESP_LOGD(TAG, "Install LCD driver");
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = BSP_LCD_RST,// 复位引脚
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,// RGB 元素顺序
        .bits_per_pixel = BSP_LCD_BITS_PER_PIXEL,// 每像素位数
    };
    ESP_GOTO_ON_ERROR(esp_lcd_new_panel_st7789(io_handle, &panel_config, &panel_handle), err, TAG, "New panel failed");// 创建新的液晶屏面
    
    esp_lcd_panel_reset(panel_handle);  // 液晶屏复位
    lcd_cs(0);  // 拉低CS引脚
    esp_lcd_panel_init(panel_handle);  // 初始化配置寄存器
    esp_lcd_panel_invert_color(panel_handle, true); // 颜色反转
    esp_lcd_panel_swap_xy(panel_handle, true);  // 显示翻转 
    esp_lcd_panel_mirror(panel_handle, true, false); // 镜像

    return ret;// 返回成功

err:
    if (panel_handle) {
        esp_lcd_panel_del(panel_handle);// 删除面板句柄
    }
    if (io_handle) {
        esp_lcd_panel_io_del(io_handle);// 删除 IO 句柄
    }
    spi_bus_free(BSP_LCD_SPI_NUM);// 释放 SPI 总线
    return ret; // 返回错误
}

//  LCD 显示初始化
esp_err_t bsp_lcd_init(void)
{
    esp_err_t ret = ESP_OK;// 初始化返回值

    
    ret = bsp_display_new(); // 液晶屏驱动初始化
    lcd_set_color(0x0000); // 设置整屏背景黑色
    ret = esp_lcd_panel_disp_on_off(panel_handle, true); // 打开液晶屏显示
    ret = bsp_display_backlight_on(); // 打开背光显示

    return  ret;// 返回结果
}

// 液晶屏初始化+添加 LVGL 接口
static lv_disp_t *bsp_display_lcd_init(void)
{
    /* 初始化液晶屏 */
    bsp_display_new(); // 液晶屏驱动初始化
    lcd_set_color(0xffff); // 设置整屏背景白色
    esp_lcd_panel_disp_on_off(panel_handle, true); // 打开液晶屏显示

    /* 液晶屏添加LVGL接口 */
    ESP_LOGD(TAG, "Add LCD screen");
    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = io_handle,// IO 句柄
        .panel_handle = panel_handle,// 面板句柄
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

    return lvgl_port_add_disp(&disp_cfg);// 添加 LVGL 显示接口
}

// 触摸屏初始化
esp_err_t bsp_touch_new(esp_lcd_touch_handle_t *ret_touch)
{
    /* 初始化触摸  */
    esp_lcd_touch_config_t tp_cfg = {
        .x_max = BSP_LCD_V_RES,// X 轴最大值
        .y_max = BSP_LCD_H_RES,// Y 轴最大值
        .rst_gpio_num = GPIO_NUM_NC, // 复位引脚（与 LCD 复位共用）
        .int_gpio_num = GPIO_NUM_NC, // 中断引脚
        .levels = {
            .reset = 0, // 复位电平
            .interrupt = 0, // 中断电平
        },
        .flags = {
            .swap_xy = 1,// 是否翻转 X 和 Y 轴
            .mirror_x = 1,// x 方向是否镜像
            .mirror_y = 0, // y 方向是否镜像
        },
    };
    esp_lcd_panel_io_handle_t tp_io_handle = NULL;// 定义触摸 IO 句柄
    esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_FT5x06_CONFIG(); // 触摸 IO 配置

    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_i2c((esp_lcd_i2c_bus_handle_t)BSP_I2C_NUM, &tp_io_config, &tp_io_handle), TAG, "");// 创建新的触摸面板 IO
    ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_ft5x06(tp_io_handle, &tp_cfg, ret_touch)); // 创建新的触摸面板

    return ESP_OK;// 返回成功
}

// 触摸屏初始化+添加LVGL接口
static lv_indev_t *bsp_display_indev_init(lv_disp_t *disp)
{
    /* 初始化触摸屏 */
    ESP_ERROR_CHECK(bsp_touch_new(&tp));// 初始化触摸屏
    assert(tp); // 确保触摸屏句柄有效

    /* 添加LVGL接口 */
    const lvgl_port_touch_cfg_t touch_cfg = {
        .disp = disp, // 指向显示器
        .handle = tp,// 触摸屏句柄
    };

    return lvgl_port_add_touch(&touch_cfg);// 添加 LVGL 触摸接口
}

// 开发板显示初始化
void bsp_lvgl_start(void)
{
    /* 初始化LVGL */
    lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();// LVGL 配置
    lvgl_port_init(&lvgl_cfg);// 初始化 LVGL

    /* 初始化液晶屏 并添加LVGL接口 */
    disp = bsp_display_lcd_init();// 初始化液晶屏并获取显示句柄

    /* 初始化触摸屏 并添加LVGL接口 */
    disp_indev = bsp_display_indev_init(disp);// 初始化触摸屏并获取输入设备句柄

    /* 打开液晶屏背光 */
    bsp_display_backlight_on();// 打开背光

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
            esp_lcd_panel_draw_bitmap(panel_handle, 0, y, 320, y+1, buffer); // 显示一行颜色
        }
        free(buffer); // 释放内存
    }
}
/***************    LCD显示屏 ↑   *************************/
/***********************************************************/


