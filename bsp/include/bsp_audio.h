/*
 * bsp_audio.h — 音频（I2S + ES8311 放音 + ES7210 4 路麦克风）
 *
 * 功放使能 PA_EN 走 PCA9557，写数据时自动打开。
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
#include "driver/i2s_std.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BSP_AUDIO_DEFAULT_SAMPLE_RATE   (16000)
#define BSP_AUDIO_DEFAULT_BIT_WIDTH     (16)
#define BSP_AUDIO_DEFAULT_CHANNELS      (2)
#define BSP_AUDIO_DEFAULT_VOLUME        (60)      /* 0~100 */
#define BSP_AUDIO_DEFAULT_ADC_GAIN_DB   (24.0f)   /* ES7210 输入增益 */

/**
 * @brief 初始化 I2S + ES8311 + ES7210（须先 bsp_board_init()）。幂等。
 */
esp_err_t bsp_audio_init(void);

/** @brief 反初始化音频 */
esp_err_t bsp_audio_deinit(void);

/**
 * @brief 重新配置采样率/位宽/声道（播放与录音同时生效）
 */
esp_err_t bsp_audio_set_fs(uint32_t rate, uint32_t bits_cfg, i2s_slot_mode_t ch);

/**
 * @brief 播放 PCM 数据（自动使能功放）
 */
esp_err_t bsp_audio_write(void *audio_buffer, size_t len, size_t *bytes_written, uint32_t timeout_ms);

/**
 * @brief 采集一帧麦克风数据（ES7210 4 通道交错排布）
 */
esp_err_t bsp_audio_read(int16_t *buffer, size_t len);

/** @brief 麦克风通道数（ES7210=4） */
int bsp_audio_feed_channels(void);

/**
 * @brief 读取供 esp-sr 使用的数据
 * @param raw_channel true=保持 4 路原始排布；false=重排为 3 路（MIC1/MIC3/参考）
 */
esp_err_t bsp_audio_feed_data(bool raw_channel, int16_t *buffer, int buffer_len);

/** @brief 静音开关 */
esp_err_t bsp_audio_mute(bool enable);

/** @brief 设置音量 0~100 */
esp_err_t bsp_audio_set_volume(int volume);

/** @brief 手动控制功放（一般无需调用，写数据自动打开） */
void bsp_audio_pa_enable(bool enable);

#ifdef __cplusplus
}
#endif
