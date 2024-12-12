#include <stdio.h> // 引入标准输入输出库
#include <string.h> // 引入字符串处理库
#include "esp_vfs_fat.h" // 引入 FAT 文件系统支持
#include "sdmmc_cmd.h" // 引入 SD/MMC 命令支持
#include "driver/sdmmc_host.h" // 引入 SD/MMC 主机驱动

// 定义 SD 卡相关引脚
#define BSP_SD_CLK          (47)// SD 卡时钟引脚
#define BSP_SD_CMD          (48)// SD 卡命令引脚
#define BSP_SD_D0           (21)// SD 卡数据引脚 D0

static const char *TAG = "main";// 定义日志标签，用于调试信息输出

// 定义挂载点和最大字符大小
#define MOUNT_POINT              "/sdcard"// SD 卡挂载点
#define EXAMPLE_MAX_CHAR_SIZE    64   // 最大字符大小定义

// 写文件内容 path是路径 data是内容
static esp_err_t s_example_write_file(const char *path, char *data)
{
    ESP_LOGI(TAG, "Opening file %s", path); // 打印正在打开的文件路径
    FILE *f = fopen(path, "w");   // 以只写方式打开路径中文件
    if (f == NULL) { // 检查文件是否成功打开
        ESP_LOGE(TAG, "Failed to open file for writing");  // 打印错误信息
        return ESP_FAIL; // 返回失败
    }
    fprintf(f, data); // 将内容写入文件
    fclose(f);  // 关闭文件
    ESP_LOGI(TAG, "File written"); // 打印文件写入成功信息

    return ESP_OK; // 返回成功
}

// 读文件内容 path是路径
static esp_err_t s_example_read_file(const char *path)
{
    ESP_LOGI(TAG, "Reading file %s", path);// 打印正在读取的文件路径
    FILE *f = fopen(path, "r");  // 以只读方式打开文件
    if (f == NULL) { // 检查文件是否成功打开
        ESP_LOGE(TAG, "Failed to open file for reading"); // 打印错误信息
        return ESP_FAIL;// 返回失败
    }
    char line[EXAMPLE_MAX_CHAR_SIZE];  // 定义一个字符串数组用于存储读取内容
    fgets(line, sizeof(line), f); // 从文件中读取一行内容到字符串数组
    fclose(f); // 关闭文件

    // 去掉换行符
    char *pos = strchr(line, '\n'); // 查找字符串中的换行符并返回其位置
    if (pos) {
        *pos = '\0'; // 把\n替换成\0  将换行符替换为字符串结束符
    }
    ESP_LOGI(TAG, "Read from file: '%s'", line); // 打印从文件读取的内容

    return ESP_OK; // 返回成功
}

void app_main(void)
{
    esp_err_t ret; // 定义返回值变量

    // 配置 SD 卡挂载参数
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = true,   // 如果挂载不成功，是否需要格式化 SD 卡
        .max_files = 5, // 允许打开的最大文件数
        .allocation_unit_size = 16 * 1024  // 分配单元大小
    };
    
    sdmmc_card_t *card; // 定义指向 SD 卡结构体的指针
    const char mount_point[] = MOUNT_POINT; // 定义挂载点字符串
    ESP_LOGI(TAG, "Initializing SD card"); // 打印 SD 卡初始化信息
    ESP_LOGI(TAG, "Using SDMMC peripheral"); // 打印使用的 SDMMC 外设信息

    // 配置 SDMMC 主机接口
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();  // 获取默认的 SDMMC 主机配置 
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT(); // 获取默认的 SDMMC 插槽配置
    slot_config.width = 1;  // 设置为 1 线 SD 模式
    slot_config.clk = BSP_SD_CLK;  // 设置时钟引脚
    slot_config.cmd = BSP_SD_CMD; // 设置命令引脚
    slot_config.d0 = BSP_SD_D0; // 设置数据引脚 D0
    slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP; // 启用内部上拉电阻

    ESP_LOGI(TAG, "Mounting filesystem");// 打印挂载文件系统信息
    ret = esp_vfs_fat_sdmmc_mount(mount_point, &host, &slot_config, &mount_config, &card); // 挂载 SD 卡

    if (ret != ESP_OK) {  // 检查挂载是否成功
        if (ret == ESP_FAIL) { // 如果挂载失败
            ESP_LOGE(TAG, "Failed to mount filesystem. ");// 打印挂载失败信息
        } else { // 如果是其它错误 打印错误名称
            ESP_LOGE(TAG, "Failed to initialize the card (%s). ", esp_err_to_name(ret));
        }
        return; // 返回，终止程序
    }
    ESP_LOGI(TAG, "Filesystem mounted"); // 打印挂载成功信息
    sdmmc_card_print_info(stdout, card); // 打印 SD 卡的一些信息到终端

    // 新建一个 txt 文件，并且给文件中写入几个字符
    const char *file_hello = MOUNT_POINT"/你好hello.txt"; // 定义文件路径
    char data[EXAMPLE_MAX_CHAR_SIZE]; // 定义数据缓冲区
    snprintf(data, EXAMPLE_MAX_CHAR_SIZE, "%s %s!\n", "你好hello先生", card->cid.name); // 格式化字符串，写入数据
    ret = s_example_write_file(file_hello, data);  // 调用写文件函数
    if (ret != ESP_OK) { // 检查写文件是否成功
        return;  // 返回，终止程序
    } 

    // 打开txt文件，并读出文件中的内容
    ret = s_example_read_file(file_hello); // 调用读文件函数
    if (ret != ESP_OK) { // 检查读文件是否成功
        return; // 返回，终止程序
    }

    // 卸载SD卡
    esp_vfs_fat_sdcard_unmount(mount_point, card); // 卸载 SD 卡
    ESP_LOGI(TAG, "Card unmounted"); // 打印卸载成功信息
}
