#include <stdio.h>
#include "esp32_s3_szp.h"
#include "logo_en_240x240_lcd.h"
#include "yingwu.h"


void app_main(void)
{
    bsp_i2c_init();  // I2C初始化：1.初始化 I2C 接口
    pca9557_init();  // IO扩展芯片初始化
    bsp_lcd_init();  // 液晶屏初始化: 1.液晶屏驱动初始化 2.设置液晶屏颜色 3. 打开背光显示
    lcd_draw_pictrue(0, 0, 320, 240, gImage_yingwu); // 显示3只鹦鹉图片
    vTaskDelay(500 / portTICK_PERIOD_MS);  // 延时500毫秒
    bsp_camera_init(); // 摄像头初始化
    app_camera_lcd(); // 让摄像头画面显示到LCD上
}
