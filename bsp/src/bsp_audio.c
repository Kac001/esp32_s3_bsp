/*
 * bsp_audio.c — 音频（I2S + ES8311 放音 + ES7210 4 路麦克风）
 *
 * 相比 demo 的修正：
 *   - I2S 时钟不再硬编码 16kHz，bsp_audio_set_fs() 会同时重配 I2S 时钟与 Codec；
 *   - ES7210 4 路麦克风全部使能（esp-sr feed 重排依赖 4 通道）；
 *   - 初始化后即设置默认音量。
 */
#include <assert.h>
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s_std.h"

#include "esp_codec_dev.h"
#include "esp_codec_dev_defaults.h"

#include "bsp_audio.h"
#include "bsp_board.h"
#include "bsp_io_expander.h"
#include "bsp_i2c.h"

static const char *TAG = "bsp_audio";

static esp_codec_dev_handle_t s_play_dev = NULL;    /* ES8311 放音 */
static esp_codec_dev_handle_t s_record_dev = NULL;  /* ES7210 录音 */
static i2s_chan_handle_t s_tx_chan = NULL;
static i2s_chan_handle_t s_rx_chan = NULL;
static const audio_codec_data_if_t *s_i2s_data_if = NULL;

static esp_err_t bsp_audio_bus_init(uint32_t rate_hz)
{
    if (s_tx_chan && s_rx_chan) {
        return ESP_OK;
    }

    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(BSP_I2S_NUM, I2S_ROLE_MASTER);
    chan_cfg.auto_clear = true;     /* 自动清空 DMA 残留数据 */
    ESP_RETURN_ON_ERROR(i2s_new_channel(&chan_cfg, &s_tx_chan, &s_rx_chan), TAG, "i2s_new_channel failed");

    const i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(rate_hz),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(32, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = BSP_I2S_MCLK,
            .bclk = BSP_I2S_BCLK,
            .ws = BSP_I2S_WS,
            .dout = BSP_I2S_DOUT,
            .din = BSP_I2S_DIN,
        },
    };

    esp_err_t ret = ESP_OK;
    if (s_tx_chan != NULL) {
        ESP_GOTO_ON_ERROR(i2s_channel_init_std_mode(s_tx_chan, &std_cfg), err, TAG, "TX init failed");
        ESP_GOTO_ON_ERROR(i2s_channel_enable(s_tx_chan), err, TAG, "TX enable failed");
    }
    if (s_rx_chan != NULL) {
        ESP_GOTO_ON_ERROR(i2s_channel_init_std_mode(s_rx_chan, &std_cfg), err, TAG, "RX init failed");
        ESP_GOTO_ON_ERROR(i2s_channel_enable(s_rx_chan), err, TAG, "RX enable failed");
    }

    audio_codec_i2s_cfg_t i2s_cfg = {
        .port = BSP_I2S_NUM,
        .rx_handle = s_rx_chan,
        .tx_handle = s_tx_chan,
    };
    s_i2s_data_if = audio_codec_new_i2s_data(&i2s_cfg);
    if (s_i2s_data_if == NULL) {
        goto err;
    }
    return ESP_OK;

err:
    if (s_tx_chan) {
        i2s_del_channel(s_tx_chan);
        s_tx_chan = NULL;
    }
    if (s_rx_chan) {
        i2s_del_channel(s_rx_chan);
        s_rx_chan = NULL;
    }
    return ret;
}

static esp_codec_dev_handle_t speaker_codec_init(void)
{
    ESP_ERROR_CHECK(bsp_audio_bus_init(BSP_AUDIO_DEFAULT_SAMPLE_RATE));
    assert(s_i2s_data_if);

    audio_codec_i2c_cfg_t i2c_cfg = {
        .port = BSP_I2C_NUM,
        .addr = ES8311_CODEC_DEFAULT_ADDR,
    };
    const audio_codec_ctrl_if_t *i2c_ctrl_if = audio_codec_new_i2c_ctrl(&i2c_cfg);
    assert(i2c_ctrl_if);

    esp_codec_dev_hw_gain_t gain = {
        .pa_voltage = 5.0,
        .codec_dac_voltage = 3.3,
    };
    es8311_codec_cfg_t es8311_cfg = {
        .ctrl_if = i2c_ctrl_if,
        .gpio_if = audio_codec_new_gpio(),
        .codec_mode = ESP_CODEC_DEV_WORK_MODE_DAC,
        .pa_pin = BSP_AUDIO_PA_PIN,
        .pa_reverted = false,
        .master_mode = false,
        .use_mclk = true,
        .digital_mic = false,
        .invert_mclk = false,
        .invert_sclk = false,
        .hw_gain = gain,
    };
    const audio_codec_if_t *es8311_dev = es8311_codec_new(&es8311_cfg);
    assert(es8311_dev);

    esp_codec_dev_cfg_t codec_cfg = {
        .dev_type = ESP_CODEC_DEV_TYPE_OUT,
        .codec_if = es8311_dev,
        .data_if = s_i2s_data_if,
    };
    return esp_codec_dev_new(&codec_cfg);
}

static esp_codec_dev_handle_t microphone_codec_init(void)
{
    ESP_ERROR_CHECK(bsp_audio_bus_init(BSP_AUDIO_DEFAULT_SAMPLE_RATE));
    assert(s_i2s_data_if);

    audio_codec_i2c_cfg_t i2c_cfg = {
        .port = BSP_I2C_NUM,
        .addr = BSP_ES7210_ADDR_8BIT,
    };
    const audio_codec_ctrl_if_t *i2c_ctrl_if = audio_codec_new_i2c_ctrl(&i2c_cfg);
    assert(i2c_ctrl_if);

    es7210_codec_cfg_t es7210_cfg = {
        .ctrl_if = i2c_ctrl_if,
        .mic_selected = ES7120_SEL_MIC1 | ES7120_SEL_MIC2 | ES7120_SEL_MIC3 | ES7120_SEL_MIC4,
    };
    const audio_codec_if_t *es7210_dev = es7210_codec_new(&es7210_cfg);
    assert(es7210_dev);

    esp_codec_dev_cfg_t codec_cfg = {
        .dev_type = ESP_CODEC_DEV_TYPE_IN,
        .codec_if = es7210_dev,
        .data_if = s_i2s_data_if,
    };
    return esp_codec_dev_new(&codec_cfg);
}

esp_err_t bsp_audio_init(void)
{
    if (s_play_dev && s_record_dev) {
        return ESP_OK;
    }

    if (s_play_dev == NULL) {
        s_play_dev = speaker_codec_init();
        ESP_RETURN_ON_FALSE(s_play_dev != NULL, ESP_FAIL, TAG, "speaker codec init failed");
    }
    if (s_record_dev == NULL) {
        s_record_dev = microphone_codec_init();
        ESP_RETURN_ON_FALSE(s_record_dev != NULL, ESP_FAIL, TAG, "mic codec init failed");
    }

    esp_err_t ret = bsp_audio_set_fs(BSP_AUDIO_DEFAULT_SAMPLE_RATE, BSP_AUDIO_DEFAULT_BIT_WIDTH,
                                     BSP_AUDIO_DEFAULT_CHANNELS == 1 ? I2S_SLOT_MODE_MONO : I2S_SLOT_MODE_STEREO);
    ESP_RETURN_ON_ERROR(ret, TAG, "set_fs failed");
    ESP_RETURN_ON_ERROR(esp_codec_dev_set_out_vol(s_play_dev, BSP_AUDIO_DEFAULT_VOLUME), TAG, "set volume failed");

    ESP_LOGI(TAG, "Audio ready (ES8311 out + ES7210 %dch in, %dHz/%dbit)",
             BSP_AUDIO_MIC_CHANNELS, BSP_AUDIO_DEFAULT_SAMPLE_RATE, BSP_AUDIO_DEFAULT_BIT_WIDTH);
    return ESP_OK;
}

esp_err_t bsp_audio_deinit(void)
{
    esp_err_t ret = ESP_OK;
    /* esp_codec_dev_delete 内部会 close 设备 */
    if (s_play_dev) {
        esp_codec_dev_delete(s_play_dev);
        s_play_dev = NULL;
    }
    if (s_record_dev) {
        esp_codec_dev_delete(s_record_dev);
        s_record_dev = NULL;
    }
    if (s_tx_chan) {
        ret |= i2s_del_channel(s_tx_chan);
        s_tx_chan = NULL;
    }
    if (s_rx_chan) {
        ret |= i2s_del_channel(s_rx_chan);
        s_rx_chan = NULL;
    }
    s_i2s_data_if = NULL;
    bsp_audio_pa_enable(false);
    return ret;
}

esp_err_t bsp_audio_set_fs(uint32_t rate, uint32_t bits_cfg, i2s_slot_mode_t ch)
{
    /* 先重配 I2S 时钟（demo 版硬编码 16kHz 的坑在此修正） */
    if (s_tx_chan || s_rx_chan) {
        const i2s_std_clk_config_t clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(rate);
        if (s_tx_chan) {
            ESP_RETURN_ON_ERROR(i2s_channel_reconfig_std_clock(s_tx_chan, &clk_cfg), TAG, "TX clk reconfig failed");
        }
        if (s_rx_chan) {
            ESP_RETURN_ON_ERROR(i2s_channel_reconfig_std_clock(s_rx_chan, &clk_cfg), TAG, "RX clk reconfig failed");
        }
    }

    esp_codec_dev_sample_info_t fs = {
        .sample_rate = rate,
        .channel = ch,
        .bits_per_sample = bits_cfg,
    };

    esp_err_t ret = ESP_OK;
    if (s_play_dev) {
        ret |= esp_codec_dev_close(s_play_dev);
        ret |= esp_codec_dev_open(s_play_dev, &fs);
    }
    if (s_record_dev) {
        ret |= esp_codec_dev_close(s_record_dev);
        ret |= esp_codec_dev_open(s_record_dev, &fs);
        ret |= esp_codec_dev_set_in_gain(s_record_dev, BSP_AUDIO_DEFAULT_ADC_GAIN_DB);
    }
    return ret;
}

esp_err_t bsp_audio_write(void *audio_buffer, size_t len, size_t *bytes_written, uint32_t timeout_ms)
{
    ESP_RETURN_ON_FALSE(s_play_dev != NULL, ESP_ERR_INVALID_STATE, TAG, "call bsp_audio_init() first");
    (void)timeout_ms;   /* esp_codec_dev_write 内部阻塞直到写完 */

    bsp_audio_pa_enable(true);
    esp_err_t ret = esp_codec_dev_write(s_play_dev, audio_buffer, len);
    if (bytes_written) {
        *bytes_written = (ret == ESP_OK) ? len : 0;
    }
    return ret;
}

esp_err_t bsp_audio_read(int16_t *buffer, size_t len)
{
    ESP_RETURN_ON_FALSE(s_record_dev != NULL, ESP_ERR_INVALID_STATE, TAG, "call bsp_audio_init() first");
    return esp_codec_dev_read(s_record_dev, (void *)buffer, len);
}

int bsp_audio_feed_channels(void)
{
    return BSP_AUDIO_MIC_CHANNELS;
}

esp_err_t bsp_audio_feed_data(bool raw_channel, int16_t *buffer, int buffer_len)
{
    ESP_RETURN_ON_FALSE(s_record_dev != NULL, ESP_ERR_INVALID_STATE, TAG, "call bsp_audio_init() first");

    int chunksize = buffer_len / (int)(sizeof(int16_t) * BSP_AUDIO_MIC_CHANNELS);
    ESP_RETURN_ON_ERROR(esp_codec_dev_read(s_record_dev, (void *)buffer, buffer_len), TAG, "read failed");

    if (!raw_channel && BSP_AUDIO_MIC_CHANNELS == 4) {
        /* 4 路交错 -> esp-sr 需要的 3 路：通道1、通道3、通道0（参考）。
         * 排布与 demo 12/14 一致（esp-sr AFE 的 3 通道输入约定） */
        for (int i = 0; i < chunksize; i++) {
            int16_t ref = buffer[4 * i + 0];
            buffer[3 * i + 0] = buffer[4 * i + 1];
            buffer[3 * i + 1] = buffer[4 * i + 3];
            buffer[3 * i + 2] = ref;
        }
    }
    return ESP_OK;
}

esp_err_t bsp_audio_mute(bool enable)
{
    ESP_RETURN_ON_FALSE(s_play_dev != NULL, ESP_ERR_INVALID_STATE, TAG, "call bsp_audio_init() first");
    return esp_codec_dev_set_out_mute(s_play_dev, enable);
}

esp_err_t bsp_audio_set_volume(int volume)
{
    ESP_RETURN_ON_FALSE(s_play_dev != NULL, ESP_ERR_INVALID_STATE, TAG, "call bsp_audio_init() first");
    if (volume < 0) {
        volume = 0;
    } else if (volume > 100) {
        volume = 100;
    }
    return esp_codec_dev_set_out_vol(s_play_dev, volume);
}

void bsp_audio_pa_enable(bool enable)
{
    bsp_ioe_write(BSP_IOE_PA_EN, enable);
}
