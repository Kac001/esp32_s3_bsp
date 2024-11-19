#include <stdio.h> //引入标准输入输出库
#include "esp32_s3_szp.h" // 引入 ESP32 S3 SZP 相关的头文件


static const char *TAG = "esp32_s3_szp"; // 定义日志标签，用于调试信息输出

/******************************************************************************/
/***************************  I2C 初始化 ↓ *******************************************/
esp_err_t bsp_i2c_init(void)// I2C 初始化函数
{
    i2c_config_t i2c_conf = { // 创建 I2C 配置结构体
        .mode = I2C_MODE_MASTER, // 设置为主模式
        .sda_io_num = BSP_I2C_SDA,  // 指定 SDA 引脚
        .sda_pullup_en = GPIO_PULLUP_ENABLE, // 启用 SDA 引脚的上拉电阻
        .scl_io_num = BSP_I2C_SCL, // 指定 SCL 引脚
        .scl_pullup_en = GPIO_PULLUP_ENABLE, // 启用 SCL 引脚的上拉电阻
        .master.clk_speed = BSP_I2C_FREQ_HZ // 设置 I2C 时钟频率
    };
    i2c_param_config(BSP_I2C_NUM, &i2c_conf); // 配置 I2C 参数

    return i2c_driver_install(BSP_I2C_NUM, i2c_conf.mode, 0, 0, 0);  // 安装 I2C 驱动
}
/***************************   I2C 初始化  ↑  *******************************************/
/*******************************************************************************/


/*******************************************************************************/
/***************************  姿态传感器 QMI8658 ↓   ****************************/

// 读取QMI8658寄存器的值
esp_err_t qmi8658_register_read(uint8_t reg_addr, uint8_t *data, size_t len)
{   
    // 通过 I2C 读取指定寄存器的值，返回操作结果
    return i2c_master_write_read_device(BSP_I2C_NUM, QMI8658_SENSOR_ADDR,  &reg_addr, 1, data, len, 1000 / portTICK_PERIOD_MS);
}

// 给QMI8658的寄存器写值
esp_err_t qmi8658_register_write_byte(uint8_t reg_addr, uint8_t data)
{
    uint8_t write_buf[2] = {reg_addr, data}; // 创建写入缓冲区，包含寄存器地址和数据

    // 通过 I2C 向指定寄存器写入数据，返回操作结果
    return i2c_master_write_to_device(BSP_I2C_NUM, QMI8658_SENSOR_ADDR, write_buf, sizeof(write_buf), 1000 / portTICK_PERIOD_MS);
}

// QMI8658
void qmi8658_init(void)
{
    uint8_t id = 0; // 芯片的ID号

    qmi8658_register_read(QMI8658_WHO_AM_I, &id ,1); // 读取 QMI8658 的 ID 寄存器
    while (id != 0x05)  // 循环判断读到的ID号是否是0x05
    {
        vTaskDelay(1000 / portTICK_PERIOD_MS);  // 延时1秒
        qmi8658_register_read(QMI8658_WHO_AM_I, &id ,1); // 读取ID号
    }
    ESP_LOGI(TAG, "QMI8658 OK!");  // 打印信息

    qmi8658_register_write_byte(QMI8658_RESET, 0xb0);  // 向复位寄存器写入值，复位传感器
    vTaskDelay(10 / portTICK_PERIOD_MS);  // 延时 10 ms，等待复位完成
    qmi8658_register_write_byte(QMI8658_CTRL1, 0x40); // 设置 CTRL1 寄存器，地址自动增加
    qmi8658_register_write_byte(QMI8658_CTRL7, 0x03); // 设置 CTRL7 寄存器，允许加速度计和陀螺仪
    qmi8658_register_write_byte(QMI8658_CTRL2, 0x95); // 设置 CTRL2 寄存器，配置加速度计为 4g，250Hz
    qmi8658_register_write_byte(QMI8658_CTRL3, 0xd5); // 设置 CTRL3 寄存器，配置陀螺仪为 512dps，250Hz 
}

// 读取加速度和陀螺仪寄存器值
void qmi8658_Read_AccAndGry(t_sQMI8658 *p)
{
    uint8_t status, data_ready=0; // 状态寄存器和数据准备标志
    int16_t buf[6]; // 用于存储加速度和陀螺仪数据的缓冲区

    qmi8658_register_read(QMI8658_STATUS0, &status, 1);     // 读取状态寄存器以检查数据是否准备好
    if (status & 0x03) // 判断加速度和陀螺仪数据是否可读
        data_ready = 1;  // 设置数据准备标志为 1
    if (data_ready == 1){  // 如果数据可读
        data_ready = 0; // 重置数据准备标志
        qmi8658_register_read(QMI8658_AX_L, (uint8_t *)buf, 12); // 读加速度和陀螺仪值
        p->acc_x = buf[0];// 保存加速度 X 轴值
        p->acc_y = buf[1];// 保存加速度 Y 轴值
        p->acc_z = buf[2];// 保存加速度 Z 轴值
        p->gyr_x = buf[3];// 保存陀螺仪 X 轴值
        p->gyr_y = buf[4];// 保存陀螺仪 Y 轴值
        p->gyr_z = buf[5];// 保存陀螺仪 Z 轴值
    }
}

// 根据加速度值计算 XYZ 轴的倾角
void qmi8658_fetch_angleFromAcc(t_sQMI8658 *p)
{
    float temp;// 临时变量用于计算

    qmi8658_Read_AccAndGry(p); // 读取加速度和陀螺仪的寄存器值
    // 根据寄存器值 计算倾角值 并把弧度转换成角度
    temp = (float)p->acc_x / sqrt( ((float)p->acc_y * (float)p->acc_y + (float)p->acc_z * (float)p->acc_z) );
    p->AngleX = atan(temp)*57.29578f; // 180/π=57.29578 计算 X 轴倾角并转换为角度
    temp = (float)p->acc_y / sqrt( ((float)p->acc_x * (float)p->acc_x + (float)p->acc_z * (float)p->acc_z) );
    p->AngleY = atan(temp)*57.29578f; // 180/π=57.29578  计算 Y 轴倾角并转换为角度
    temp = sqrt( ((float)p->acc_x * (float)p->acc_x + (float)p->acc_y * (float)p->acc_y) ) / (float)p->acc_z;
    p->AngleZ = atan(temp)*57.29578f; // 180/π=57.29578 计算 Z 轴倾角并转换为角度
}
/***************************  姿态传感器 QMI8658 ↑  ****************************/
/*******************************************************************************/


/***********************************************************/
/***************    IO扩展芯片 ↓   *************************/

// 读取PCA9557寄存器的值
esp_err_t pca9557_register_read(uint8_t reg_addr, uint8_t *data, size_t len)
{
    // 通过 I2C 读取指定寄存器的值，返回操作结果
    return i2c_master_write_read_device(BSP_I2C_NUM, PCA9557_SENSOR_ADDR,  &reg_addr, 1, data, len, 1000 / portTICK_PERIOD_MS);
}

// 给PCA9557的寄存器写值
esp_err_t pca9557_register_write_byte(uint8_t reg_addr, uint8_t data)
{
    uint8_t write_buf[2] = {reg_addr, data};// 创建写入缓冲区，包含寄存器地址和数据

    // 通过 I2C 向指定寄存器写入数据，返回操作结果
    return i2c_master_write_to_device(BSP_I2C_NUM, PCA9557_SENSOR_ADDR, write_buf, sizeof(write_buf), 1000 / portTICK_PERIOD_MS);
}

// 初始化PCA9557 IO扩展芯片
void pca9557_init(void)
{
    // 写入控制引脚默认值 DVP_PWDN=1  PA_EN = 0  LCD_CS = 1，PCA9557_OUTPUT_PORT是输出电平设置口，参数0x05表示0000 0101 根据图纸， 后面101是设置io0-io2的电平 看数据手册(https://www.nxp.com.cn/docs/en/data-sheet/PCA9557.pdf)
    pca9557_register_write_byte(PCA9557_OUTPUT_PORT, 0x05);  
    // 把PCA9557芯片的IO1 IO1 IO2设置为输出 其它引脚保持默认的输入
    pca9557_register_write_byte(PCA9557_CONFIGURATION_PORT, 0xf8); 
}

// 设置 PCA9557 芯片的某个 IO 引脚输出高低电平
esp_err_t pca9557_set_output_state(uint8_t gpio_bit, uint8_t level)
{
    uint8_t data; // 存储当前输出状态的变量
    esp_err_t res = ESP_FAIL;  // 初始化返回值为失败

    // 读取当前输出状态
    pca9557_register_read(PCA9557_OUTPUT_PORT, &data, 1);
    // 更新指定 GPIO 引脚的状态，并写入 PCA9557
    res = pca9557_register_write_byte(PCA9557_OUTPUT_PORT, SET_BITS(data, gpio_bit, level));

    return res; // 返回操作结果
}

// 控制 PCA9557_LCD_CS 引脚输出高低电平 参数0输出低电平 参数1输出高电平 
void lcd_cs(uint8_t level)
{
    pca9557_set_output_state(LCD_CS_GPIO, level); // 设置 LCD_CS 引脚状态
}

// 控制 PCA9557_PA_EN 引脚输出高低电平 参数0输出低电平 参数1输出高电平 
void pa_en(uint8_t level)
{
    pca9557_set_output_state(PA_EN_GPIO, level);  // 设置 PA_EN 引脚状态
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

// 背光PWM初始化
esp_err_t bsp_display_brightness_init(void)
{
    // 设置 LEDC 外设以控制 PWM 背光
    const ledc_channel_config_t LCD_backlight_channel = {
        .gpio_num = BSP_LCD_BACKLIGHT,  // 指定背光控制引脚
        .speed_mode = LEDC_LOW_SPEED_MODE,// 设置为低速模式
        .channel = LCD_LEDC_CH, // 指定 LEDC 通道
        .intr_type = LEDC_INTR_DISABLE,// 禁用中断
        .timer_sel = 1, // 选择定时器
        .duty = 0,// 初始占空比为 0
        .hpoint = 0,// 高点为 0
        .flags.output_invert = true// 输出反转
    };
    //配置 LEDC（LED 控制器）定时器
    const ledc_timer_config_t LCD_backlight_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE, // 设置为低速模式
        .duty_resolution = LEDC_TIMER_10_BIT,// 设置占空比分辨率为 10 位
        .timer_num = 1,// 选择定时器
        .freq_hz = 5000,// 设置频率为 5000 Hz
        .clk_cfg = LEDC_AUTO_CLK// 使用自动时钟配置
    };

    ESP_ERROR_CHECK(ledc_timer_config(&LCD_backlight_timer)); // 配置 LEDC 定时器
    ESP_ERROR_CHECK(ledc_channel_config(&LCD_backlight_channel)); // 配置 LEDC 通道

    return ESP_OK;
}

// 背光亮度设置
esp_err_t bsp_display_brightness_set(int brightness_percent)
{   
     // 限制亮度百分比在 0 到 100 之间
    if (brightness_percent > 100) {
        brightness_percent = 100;// 最大值为 100%
    } else if (brightness_percent < 0) {
        brightness_percent = 0; // 最小值为 0%
    }

    ESP_LOGI(TAG, "Setting LCD backlight: %d%%", brightness_percent); // 打印设置的亮度信息
    // LEDC 分辨率设置为 10 位，因此：100% = 1023
    uint32_t duty_cycle = (1023 * brightness_percent) / 100; // 计算对应的占空比
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, LCD_LEDC_CH, duty_cycle));// 设置占空比
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, LCD_LEDC_CH)); // 更新占空比

    return ESP_OK; // 返回成功
}

// 关闭背光
esp_err_t bsp_display_backlight_off(void)
{
    return bsp_display_brightness_set(0);// 设置背光亮度为 0，关闭背光
}

// 打开背光 最亮
esp_err_t bsp_display_backlight_on(void)
{
    return bsp_display_brightness_set(100);// 设置背光亮度为 100%，打开背光
}

static esp_lcd_panel_handle_t panel_handle = NULL;// 定义液晶面板句柄

// 液晶屏初始化
esp_err_t bsp_display_new(void)
{
    esp_lcd_panel_io_handle_t io_handle = NULL; // 定义液晶面板 IO 句柄
    esp_err_t ret = ESP_OK;// 初始化返回值为成功

    // 背光初始化
    ESP_RETURN_ON_ERROR(bsp_display_brightness_init(), TAG, "Brightness init failed");  // 初始化背光，检查返回值

    // 初始化SPI总线
    ESP_LOGD(TAG, "Initialize SPI bus");  // 打印 SPI 总线初始化信息
    const spi_bus_config_t buscfg = {
        .sclk_io_num = BSP_LCD_SPI_CLK,  // 指定 SPI 时钟引脚
        .mosi_io_num = BSP_LCD_SPI_MOSI,// 指定 SPI 主输出从输入引脚
        .miso_io_num = GPIO_NUM_NC,// 指定 SPI 主输入从输出引脚（未连接）
        .quadwp_io_num = GPIO_NUM_NC,// 指定四线写保护引脚（未连接）
        .quadhd_io_num = GPIO_NUM_NC, // 指定四线数据保持引脚（未连接）
        .max_transfer_sz = BSP_LCD_H_RES * BSP_LCD_V_RES * sizeof(uint16_t),// 设置最大传输大小
    };
    ESP_RETURN_ON_ERROR(spi_bus_initialize(BSP_LCD_SPI_NUM, &buscfg, SPI_DMA_CH_AUTO), TAG, "SPI init failed"); // 初始化 SPI 总线，检查返回


    // 液晶屏控制IO初始化
    ESP_LOGD(TAG, "Install panel IO");// 打印液晶屏 IO 安装信息
    const esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = BSP_LCD_DC,  // 指定数据/命令引脚
        .cs_gpio_num = BSP_LCD_SPI_CS,// 指定片选引脚
        .pclk_hz = BSP_LCD_PIXEL_CLOCK_HZ,// 指定像素时钟频率
        .lcd_cmd_bits = LCD_CMD_BITS,// 指定命令位数
        .lcd_param_bits = LCD_PARAM_BITS, // 指定参数位数
        .spi_mode = 2,// 设置 SPI 模式
        .trans_queue_depth = 10,// 设置传输队列深度
    };
    // 创建新的液晶面板 IO 句柄
    ESP_GOTO_ON_ERROR(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)BSP_LCD_SPI_NUM, &io_config, &io_handle), err, TAG, "New panel IO failed");


    // 初始化液晶屏驱动芯片ST7789
    ESP_LOGD(TAG, "Install LCD driver");// 打印液晶屏驱动安装信息
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = BSP_LCD_RST,// 指定复位引脚
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,// 设置 RGB 元素顺序
        .bits_per_pixel = BSP_LCD_BITS_PER_PIXEL,// 指定每个像素的位数
    };
    ESP_GOTO_ON_ERROR(esp_lcd_new_panel_st7789(io_handle, &panel_config, &panel_handle), err, TAG, "New panel failed");
    
    esp_lcd_panel_reset(panel_handle);  // 液晶屏复位
    lcd_cs(0);  // 拉低 CS 引脚，选择液晶屏
    esp_lcd_panel_init(panel_handle);  // 初始化液晶屏配置寄存器
    esp_lcd_panel_invert_color(panel_handle, true); // 颜色反转
    esp_lcd_panel_swap_xy(panel_handle, true);  // 显示翻转 
    esp_lcd_panel_mirror(panel_handle, true, false); // 镜像

    return ret; // 返回成功

err:
    // 错误处理，释放资源
    if (panel_handle) {
        esp_lcd_panel_del(panel_handle);  // 删除液晶面板句柄
    }
    if (io_handle) {
        esp_lcd_panel_io_del(io_handle);  // 删除液晶面板 IO 句柄
    }
    spi_bus_free(BSP_LCD_SPI_NUM);  // 释放 SPI 总线
    return ret; // 返回错误
}

// LCD显示初始化
esp_err_t bsp_lcd_init(void)
{
    esp_err_t ret = ESP_OK;  // 初始化返回值为成功

    
    ret = bsp_display_new(); // 液晶屏驱动初始化
    lcd_set_color(0x0000); // 设置整屏背景黑色
    ret = esp_lcd_panel_disp_on_off(panel_handle, true); // 打开液晶屏显示
    ret = bsp_display_backlight_on(); // 打开背光显示

    return  ret;  // 返回初始化结果
}

// 显示图片
void lcd_draw_pictrue(int x_start, int y_start, int x_end, int y_end, const unsigned char *gImage)
{
    // 计算需要的字节大小，分配内存
    size_t pixels_byte_size = (x_end - x_start)*(y_end - y_start) * 2; // 每个像素占 2 字节
    uint16_t *pixels = (uint16_t *)heap_caps_malloc(pixels_byte_size, MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM); // 在 SPIRAM 中分配内存
    if (NULL == pixels)  // 检查内存分配是否成功
    {
        ESP_LOGE(TAG, "Memory for bitmap is not enough"); // 打印错误信息
        return;// 退出函数
    }
    memcpy(pixels, gImage, pixels_byte_size);  // 把图片数据拷贝到内存
    esp_lcd_panel_draw_bitmap(panel_handle, x_start, y_start, x_end, y_end, (uint16_t *)pixels); // 显示整张图片数据
    heap_caps_free(pixels);  // 释放内存
}

// 设置液晶屏颜色
void lcd_set_color(uint16_t color)
{
    // 分配内存 这里分配了液晶屏一行数据需要的大小
    uint16_t *buffer = (uint16_t *)heap_caps_malloc(BSP_LCD_H_RES * sizeof(uint16_t), MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);
    
    if (NULL == buffer)// 检查内存分配是否成功
    {
        ESP_LOGE(TAG, "Memory for bitmap is not enough"); // 打印错误信息
    }
    else
    {   
        // 给缓存中放入颜色数据
        for (size_t i = 0; i < BSP_LCD_H_RES; i++)  // 遍历每一列
        {
            buffer[i] = color;// 将颜色值写入缓存
        }
        // 显示整屏颜色
        for (int y = 0; y < 240; y++) // 遍历每一行
        {
            esp_lcd_panel_draw_bitmap(panel_handle, 0, y, 320, y+1, buffer); // 绘制一行
        }
        free(buffer); // 释放内存
    }
}
/***************    LCD显示屏 ↑   *************************/
/***********************************************************/



