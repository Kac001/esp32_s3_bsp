#include <stdio.h>
#include "esp32_s3_szp.h"
#include "app_ui.h"


void app_main(void)
{
    bsp_i2c_init();  // I2C初始化
    pca9557_init();  // IO扩展芯片初始化
    bsp_lvgl_start(); // 初始化液晶屏lvgl接口

    bsp_spiffs_mount(); // SPIFFS文件系统初始化
    bsp_codec_init(); // 音频初始化
    mp3_player_init(); // MP3播放器初始化

    /*
    1.bsp_spiffs_mount SPIFFS文件系统初始化

    2.bsp_codec_init 音频初始化
        bsp_audio_codec_speaker_init // 初始化音频输出设备
            bsp_audio_init  I2S总线初始化

        bsp_audio_codec_microphone_init // 初始化音频输入设备
            bsp_audio_init  I2S总线初始化

        bsp_codec_set_fs // 设置采样率
    
    3.mp3_player_init // MP3播放器初始化
        获取文件信息
        初始化音频播放
        music_ui 显示界面
            btn_play_pause_cb 播放暂停事件
            btn_prev_next_cb 上一首 下一首事件
            music_list_cb 音乐列表事件
    
    */
}
