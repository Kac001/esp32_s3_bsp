#include "esp32_s3_szp.h" // 包含 ESP32 S3 SZP 的头文件

static const char *TAG = "esp32_s3_szp"; // 定义日志标签

/******************************************************************************/
/***************************  I2C 初始化函数  ↓ *******************************************/
esp_err_t bsp_i2c_init(void)
{
    i2c_config_t i2c_conf = { // 创建 I2C 配置结构体
        .mode = I2C_MODE_MASTER, // 设置为主模式
        .sda_io_num = BSP_I2C_SDA, // 指定 SDA 引脚
        .sda_pullup_en = GPIO_PULLUP_ENABLE, // 启用 SDA 上拉电阻
        .scl_io_num = BSP_I2C_SCL,  // 指定 SCL 引脚
        .scl_pullup_en = GPIO_PULLUP_ENABLE, // 启用 SCL 上拉电阻
        .master.clk_speed = BSP_I2C_FREQ_HZ // 设置 I2C 时钟频率
    };
    i2c_param_config(BSP_I2C_NUM, &i2c_conf); // 配置 I2C 参数

    return i2c_driver_install(BSP_I2C_NUM, i2c_conf.mode, 0, 0, 0); // 安装 I2C 驱动
} 
/***************************  I2C 初始化函数 ↑  *******************************************/
/*******************************************************************************/


/*******************************************************************************/
/***************************  姿态传感器 QMI8658 ↓   ****************************/

// 读取 QMI8658 寄存器的值
esp_err_t qmi8658_register_read(uint8_t reg_addr, uint8_t *data, size_t len)
{   
    // 通过 I2C 读取指定寄存器的值
    return i2c_master_write_read_device(BSP_I2C_NUM, QMI8658_SENSOR_ADDR,  &reg_addr, 1, data, len, 1000 / portTICK_PERIOD_MS);
}

// 向 QMI8658 的寄存器写入一个字节
esp_err_t qmi8658_register_write_byte(uint8_t reg_addr, uint8_t data)
{
    uint8_t write_buf[2] = {reg_addr, data};// 创建写入缓冲区，包含寄存器地址和数据
    // 通过 I2C 向指定寄存器写入数据
    return i2c_master_write_to_device(BSP_I2C_NUM, QMI8658_SENSOR_ADDR, write_buf, sizeof(write_buf), 1000 / portTICK_PERIOD_MS);
}

//  初始化 QMI8658 传感器
void qmi8658_init(void)
{
    uint8_t id = 0; // 芯片的ID号

    qmi8658_register_read(QMI8658_WHO_AM_I, &id ,1); // 读取 QMI8658 的 ID 寄存器
    while (id != 0x05)  // 判断读到的ID号是否是0x05，循环直到读取到正确的 ID
    {
        vTaskDelay(1000 / portTICK_PERIOD_MS);  // 延时1秒
        qmi8658_register_read(QMI8658_WHO_AM_I, &id ,1); // 重新读取ID号
    }
    ESP_LOGI(TAG, "QMI8658 OK!");  // 打印传感器初始化成功信息

    // 配置 QMI8658 寄存器
    qmi8658_register_write_byte(QMI8658_RESET, 0xb0);  // 向复位寄存器写入值，复位传感器 
    vTaskDelay(10 / portTICK_PERIOD_MS);  // 延时10ms，等待复位完成
    qmi8658_register_write_byte(QMI8658_CTRL1, 0x40); // 设置 CTRL1 寄存器，地址自动增加
    qmi8658_register_write_byte(QMI8658_CTRL7, 0x03); // 设置 CTRL7 寄存器，允许加速度计和陀螺仪
    qmi8658_register_write_byte(QMI8658_CTRL2, 0x95); // 设置 CTRL2 寄存器，配置加速度计为 4g，250Hz
    qmi8658_register_write_byte(QMI8658_CTRL3, 0xd5); // 设置 CTRL3 寄存器，配置陀螺仪为 512dps，250Hz 
}

// 读取加速度和陀螺仪寄存器值
void qmi8658_Read_AccAndGry(t_sQMI8658 *p)
{
    uint8_t status, data_ready=0; // 定义状态寄存器和数据准备标志
    int16_t buf[6]; // 用于存储加速度和陀螺仪数据的缓冲区

    qmi8658_register_read(QMI8658_STATUS0, &status, 1); // 读取状态寄存器、以检查数据是否准备好
    if (status & 0x03) // 判断加速度和陀螺仪数据是否可读
        data_ready = 1; // 设置数据准备标志为 1
    if (data_ready == 1){  // 如果数据可读
        data_ready = 0; // 重置数据准备标志
        qmi8658_register_read(QMI8658_AX_L, (uint8_t *)buf, 12); // 读加速度和陀螺仪值
        p->acc_x = buf[0];  // 保存加速度 X 轴值
        p->acc_y = buf[1];  // 保存加速度 Y 轴值
        p->acc_z = buf[2];  // 保存加速度 Z 轴值
        p->gyr_x = buf[3];  // 保存陀螺仪 X 轴值
        p->gyr_y = buf[4];  // 保存陀螺仪 Y 轴值
        p->gyr_z = buf[5];  // 保存陀螺仪 Z 轴值
    }
}

// 根据加速度值计算 XYZ 轴的倾角
void qmi8658_fetch_angleFromAcc(t_sQMI8658 *p)
{
    float temp; // 临时变量用于计算

    qmi8658_Read_AccAndGry(p); // 读取加速度和陀螺仪的寄存器值
    // 根据寄存器值 计算倾角值 并把弧度转换成角度
    temp = (float)p->acc_x / sqrt( ((float)p->acc_y * (float)p->acc_y + (float)p->acc_z * (float)p->acc_z) );
    p->AngleX = atan(temp)*57.29578f; // 180/π=57.29578 计算 X 轴倾角并转换为角度
    temp = (float)p->acc_y / sqrt( ((float)p->acc_x * (float)p->acc_x + (float)p->acc_z * (float)p->acc_z) );
    p->AngleY = atan(temp)*57.29578f; // 180/π=57.29578 计算 Y 轴倾角并转换为角度
    temp = sqrt( ((float)p->acc_x * (float)p->acc_x + (float)p->acc_y * (float)p->acc_y) ) / (float)p->acc_z;
    p->AngleZ = atan(temp)*57.29578f; // 180/π=57.29578 计算 Z 轴倾角并转换为角度
}
/***************************  姿态传感器 QMI8658 ↑  ****************************/
/*******************************************************************************/
