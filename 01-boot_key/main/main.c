#include <stdio.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"

static QueueHandle_t gpio_evt_queue = NULL;  // 定义用于GPIO事件的队列句柄


// GPIO中断服务函数(点击按钮就会触发)，发送消息给队列
static void IRAM_ATTR gpio_isr_handler(void* arg) 
{
    uint32_t gpio_num = (uint32_t) arg;   // 获取触发中断的GPIO编号
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL); // 将GPIO编号发送到事件队列
}

// GPIO任务处理函数，从队列处理消息
static void gpio_task_example(void* arg)
{
    uint32_t io_num; // 用于存储接收到的GPIO编号 表示哪个gpio
    for(;;) {
          // 阻塞等待，从队列接收GPIO事件并处理
        if(xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY)) {  // 死等队列消息
            printf("GPIO[%"PRIu32"] intr, val: %d\n", io_num, gpio_get_level(io_num)); // 打印GPIO编号及其当前电平状态
        }
    }
}

void app_main(void)
{
    // 配置GPIO0的输入模式及中断类型
    gpio_config_t io0_conf = {
        .intr_type = GPIO_INTR_NEGEDGE, // 下降沿中断
        .mode = GPIO_MODE_INPUT, // 输入模式
        .pin_bit_mask = 1<<GPIO_NUM_0, // 选择GPIO0
        .pull_down_en = 0, // 禁能内部下拉
        .pull_up_en = 1 // 使能内部上拉
    };

    // 根据配置初始化GPIO0
    gpio_config(&io0_conf);

    // 创建一个用于存储GPIO事件的队列，队列长度为10
    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));

    // 创建GPIO事件处理任务
    xTaskCreate(gpio_task_example, "gpio_task_example", 2048, NULL, 10, NULL);

    // 安装GPIO中断服务
    gpio_install_isr_service(0);

    // 为GPIO0添加中断处理函数
    gpio_isr_handler_add(GPIO_NUM_0, gpio_isr_handler, (void*) GPIO_NUM_0);
}
