/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "esp_log.h"

#include "esp_log.h"
#include "esp_err.h"
#include "esp_gmf_element.h"
#include "esp_gmf_pipeline.h"
#include "esp_gmf_pool.h"
#include "esp_heap_caps.h"  // 直接包含ESP堆管理头文件

#include "esp_gmf_io_file.h"
#include "esp_gmf_io_http.h"
#include "esp_gmf_io_embed_flash.h"
#include "esp_gmf_copier.h"
#include "esp_gmf_setup_pool.h"

#ifdef USE_ESP_GMF_ESP_CODEC_DEV_IO
#include "esp_gmf_io_codec_dev.h"
#include "esp_gmf_io_i2s_pdm.h"
#include "driver/i2s_pdm.h"
#endif  /* USE_ESP_GMF_ESP_CODEC_DEV_IO */

#include "esp_gmf_ch_cvt.h"
#include "esp_gmf_bit_cvt.h"
#include "esp_gmf_rate_cvt.h"
#include "esp_gmf_sonic.h"
#include "esp_gmf_alc.h"
#include "esp_gmf_eq.h"
#include "esp_gmf_fade.h"
#include "esp_gmf_mixer.h"
#include "esp_gmf_interleave.h"
#include "esp_gmf_deinterleave.h"
#include "esp_gmf_audio_dec.h"
#include "esp_audio_simple_dec_default.h"

#include "esp_audio_enc_default.h"
#include "esp_gmf_audio_enc.h"

#include "esp_audio_dec_default.h"
#include "esp_audio_dec_reg.h"
#include "esp_http_client.h"
#include "esp_gmf_gpio_config.h"
#include "esp_gmf_audio_helper.h"
#include "driver/i2c_master.h"

static const char *TAG = "ESP_GMF_SETUP_POOL";

// 自定义内存分配函数，优先使用PSRAM
static void* gmf_malloc_psram(size_t size)
{
    ESP_LOGI(TAG, "Allocating %d bytes, trying PSRAM first", size);
    void* ptr = heap_caps_malloc(size, MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);
    if (ptr == NULL) {
        ESP_LOGE(TAG, "Failed to allocate %d bytes from PSRAM, trying internal memory", size);
        ptr = heap_caps_malloc(size, MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
        if (ptr != NULL) {
            ESP_LOGW(TAG, "Allocated %d bytes from internal memory", size);
        } else {
            ESP_LOGE(TAG, "Failed to allocate %d bytes from any memory!", size);
        }
    } else {
        ESP_LOGI(TAG, "Successfully allocated %d bytes from PSRAM", size);
    }
    return ptr;
}

// 替换所有内存分配函数
#define esp_gmf_oal_mem_malloc(size) gmf_malloc_psram(size)

#define SETUP_AUDIO_SAMPLE_RATE 16000
#define SETUP_AUDIO_BITS        16
#define SETUP_AUDIO_CHANNELS    1

static const char *header_type[] = {
    "audio/aac",
    "audio/opus",
    "audio/wav",
};

#ifdef USE_ESP_GMF_ESP_CODEC_DEV_IO
i2s_chan_handle_t pdm_tx_chan = NULL;
#endif  /* USE_ESP_GMF_ESP_CODEC_DEV_IO */

static esp_err_t _http_stream_event_handle(http_stream_event_msg_t *msg)
{
    esp_http_client_handle_t http = (esp_http_client_handle_t)msg->http_client;
    char len_buf[16];
    static int total_write = 0;

    if (msg->event_id == HTTP_STREAM_PRE_REQUEST) {
        // set header
        ESP_LOGE(TAG, "[ + ] HTTP client HTTP_STREAM_PRE_REQUEST, length=%d, format:%s", msg->buffer_len, (char *)msg->user_data);
        esp_http_client_set_method(http, HTTP_METHOD_POST);
        char dat[10] = {0};
        snprintf(dat, sizeof(dat), "%d", SETUP_AUDIO_SAMPLE_RATE);
        esp_http_client_set_header(http, "x-audio-sample-rates", dat);
        esp_audio_type_t fmt = 0;
        esp_gmf_audio_helper_get_audio_type_by_uri((char *)msg->user_data, &fmt);
        if (fmt == ESP_AUDIO_TYPE_AAC) {
            esp_http_client_set_header(http, "Content-Type", header_type[0]);
        } else if (fmt == ESP_AUDIO_TYPE_OPUS) {
            esp_http_client_set_header(http, "Content-Type", header_type[1]);
        } else {
            esp_http_client_set_header(http, "Content-Type", header_type[2]);
        }

        memset(dat, 0, sizeof(dat));
        snprintf(dat, sizeof(dat), "%d", SETUP_AUDIO_BITS);
        esp_http_client_set_header(http, "x-audio-bits", dat);
        memset(dat, 0, sizeof(dat));
        snprintf(dat, sizeof(dat), "%d", SETUP_AUDIO_CHANNELS);
        esp_http_client_set_header(http, "x-audio-channel", dat);
        total_write = 0;
        return ESP_OK;
    }

    if (msg->event_id == HTTP_STREAM_ON_REQUEST) {
        // write data
        int wlen = sprintf(len_buf, "%x\r\n", msg->buffer_len);
        if (esp_http_client_write(http, len_buf, wlen) <= 0) {
            return ESP_FAIL;
        }
        if (esp_http_client_write(http, msg->buffer, msg->buffer_len) <= 0) {
            return ESP_FAIL;
        }
        if (esp_http_client_write(http, "\r\n", 2) <= 0) {
            return ESP_FAIL;
        }
        total_write += msg->buffer_len;
        printf("\033[A\33[2K\rTotal bytes written: %d\n", total_write);
        return msg->buffer_len;
    }

    if (msg->event_id == HTTP_STREAM_POST_REQUEST) {
        ESP_LOGE(TAG, "[ + ] HTTP client HTTP_STREAM_POST_REQUEST, write end chunked marker");
        if (esp_http_client_write(http, "0\r\n\r\n", 5) <= 0) {
            return ESP_FAIL;
        }
        return ESP_OK;
    }

    if (msg->event_id == HTTP_STREAM_FINISH_REQUEST) {
        ESP_LOGE(TAG, "[ + ] HTTP client HTTP_STREAM_FINISH_REQUEST");
        char *buf = calloc(1, 64);
        assert(buf);
        int read_len = esp_http_client_read(http, buf, 64);
        if (read_len <= 0) {
            free(buf);
            return ESP_FAIL;
        }
        buf[read_len] = 0;
        ESP_LOGI(TAG, "Got HTTP Response = %s", (char *)buf);
        free(buf);
        total_write = 0;
        return ESP_OK;
    }
    return ESP_OK;
}

void pool_register_i2s_pdm_tx(esp_gmf_pool_handle_t pool, uint32_t sample_rate, uint8_t bits, uint8_t channel)
{
#ifdef USE_ESP_GMF_ESP_CODEC_DEV_IO
    /* Setp 1: Determine the I2S channel configuration and allocate TX channel only
     * The default configuration can be generated by the helper macro,
     * it only requires the I2S controller id and I2S role,
     * but note that PDM channel can only be registered on I2S_NUM_0 */
    i2s_chan_config_t tx_chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
    tx_chan_cfg.auto_clear = true;
    tx_chan_cfg.dma_desc_num = 10;
    tx_chan_cfg.dma_frame_num = 900;
    ESP_ERROR_CHECK(i2s_new_channel(&tx_chan_cfg, &pdm_tx_chan, NULL));

    /* Step 2: Setting the configurations of PDM TX mode and initialize the TX channel
     * The slot configuration and clock configuration can be generated by the macros
     * These two helper macros is defined in 'i2s_pdm.h' which can only be used in PDM TX mode.
     * They can help to specify the slot and clock configurations for initialization or re-configuring */
    i2s_pdm_tx_config_t pdm_tx_cfg = {
#if CONFIG_EXAMPLE_PDM_TX_DAC
        .clk_cfg = I2S_PDM_TX_CLK_DAC_DEFAULT_CONFIG(sample_rate),
        /* The data bit-width of PDM mode is fixed to 16 */
        .slot_cfg = I2S_PDM_TX_SLOT_DAC_DEFAULT_CONFIG(bits, channel),
#else
        .clk_cfg = I2S_PDM_TX_CLK_DEFAULT_CONFIG(sample_rate),
        /* The data bit-width of PDM mode is fixed to 16 */
        .slot_cfg = I2S_PDM_TX_SLOT_DEFAULT_CONFIG(bits, channel),
#endif  /* CONFIG_EXAMPLE_PDM_TX_DAC */
        .gpio_cfg = {
            .clk = ESP_GMF_I2S_DAC_BCLK_IO_NUM,
            .dout = ESP_GMF_I2S_DAC_DO_IO_NUM,
            .invert_flags = {
                .clk_inv = false,
            },
        },
    };
    ESP_ERROR_CHECK(i2s_channel_init_pdm_tx_mode(pdm_tx_chan, &pdm_tx_cfg));

    i2s_pdm_io_cfg_t pdm_cfg = ESP_GMF_IO_I2S_PDM_CFG_DEFAULT();
    pdm_cfg.dir = ESP_GMF_IO_DIR_WRITER;
    pdm_cfg.pdm_chan = pdm_tx_chan;
    esp_gmf_io_handle_t pdm = NULL;
    esp_gmf_io_i2s_pdm_init(&pdm_cfg, &pdm);
    esp_gmf_pool_register_io(pool, pdm, NULL);
#endif  /* USE_ESP_GMF_ESP_CODEC_DEV_IO */
}

void pool_unregister_i2s_pdm_tx(void)
{
#ifdef USE_ESP_GMF_ESP_CODEC_DEV_IO
    i2s_del_channel(pdm_tx_chan);
#endif  /* USE_ESP_GMF_ESP_CODEC_DEV_IO */
}

void pool_register_codec_dev_io(esp_gmf_pool_handle_t pool, void *play_dev, void *record_dev)
{
#ifdef USE_ESP_GMF_ESP_CODEC_DEV_IO
    if (play_dev != NULL) {
        codec_dev_io_cfg_t tx_codec_dev_cfg = ESP_GMF_IO_CODEC_DEV_CFG_DEFAULT();
        tx_codec_dev_cfg.dir = ESP_GMF_IO_DIR_WRITER;
        tx_codec_dev_cfg.dev = play_dev;
        tx_codec_dev_cfg.name = "codec_dev_tx";
        esp_gmf_io_handle_t tx_dev = NULL;
        esp_gmf_io_codec_dev_init(&tx_codec_dev_cfg, &tx_dev);
        esp_gmf_pool_register_io(pool, tx_dev, NULL);
    }
    if (record_dev != NULL) {
        codec_dev_io_cfg_t rx_codec_dev_cfg = ESP_GMF_IO_CODEC_DEV_CFG_DEFAULT();
        rx_codec_dev_cfg.dir = ESP_GMF_IO_DIR_READER;
        rx_codec_dev_cfg.dev = record_dev;
        rx_codec_dev_cfg.name = "codec_dev_rx";
        esp_gmf_io_handle_t rx_dev = NULL;
        esp_gmf_io_codec_dev_init(&rx_codec_dev_cfg, &rx_dev);
        esp_gmf_pool_register_io(pool, rx_dev, NULL);
    }
#endif  /* USE_ESP_GMF_ESP_CODEC_DEV_IO */
}

void pool_register_io(esp_gmf_pool_handle_t pool)
{
    http_io_cfg_t http_cfg = HTTP_STREAM_CFG_DEFAULT();
    esp_gmf_io_handle_t http = NULL;
    http_cfg.dir = ESP_GMF_IO_DIR_WRITER;
    http_cfg.event_handle = _http_stream_event_handle;
    esp_gmf_io_http_init(&http_cfg, &http);
    esp_gmf_pool_register_io(pool, http, NULL);

    http_cfg.dir = ESP_GMF_IO_DIR_READER;
    http_cfg.event_handle = NULL;
    http = NULL;
    esp_gmf_io_http_init(&http_cfg, &http);
    esp_gmf_pool_register_io(pool, http, NULL);

    file_io_cfg_t fs_cfg = FILE_IO_CFG_DEFAULT();
    fs_cfg.dir = ESP_GMF_IO_DIR_READER;
    esp_gmf_io_handle_t fs = NULL;
    esp_gmf_io_file_init(&fs_cfg, &fs);
    esp_gmf_pool_register_io(pool, fs, NULL);

    fs_cfg.dir = ESP_GMF_IO_DIR_WRITER;
    esp_gmf_io_file_init(&fs_cfg, &fs);
    esp_gmf_pool_register_io(pool, fs, NULL);

    esp_gmf_copier_cfg_t copier_cfg = {
        .copy_num = 1,
    };
    esp_gmf_element_handle_t copier = NULL;
    esp_gmf_copier_init(&copier_cfg, &copier);
    esp_gmf_pool_register_element(pool, copier, NULL);

    embed_flash_io_cfg_t flash_cfg = EMBED_FLASH_CFG_DEFAULT();
    esp_gmf_io_handle_t flash = NULL;
    esp_gmf_io_embed_flash_init(&flash_cfg, &flash);
    esp_gmf_pool_register_io(pool, flash, NULL);
}

void pool_register_audio_codecs(esp_gmf_pool_handle_t pool)
{
    esp_audio_enc_register_default();
    esp_audio_enc_config_t es_enc_cfg = DEFAULT_ESP_GMF_AUDIO_ENC_CONFIG();
    esp_aac_enc_config_t aac_enc_cfg = ESP_AAC_ENC_CONFIG_DEFAULT();
    aac_enc_cfg.sample_rate = 16000;
    aac_enc_cfg.channel = 1;
    aac_enc_cfg.bits_per_sample = 16;
    aac_enc_cfg.bitrate = 64000;
    aac_enc_cfg.adts_used = true;
    es_enc_cfg.type = ESP_AUDIO_TYPE_AAC;
    es_enc_cfg.cfg = &aac_enc_cfg;
    es_enc_cfg.cfg_sz = sizeof(esp_aac_enc_config_t);
    esp_gmf_element_handle_t enc_handle = NULL;
    esp_gmf_audio_enc_init(&es_enc_cfg, &enc_handle);
    esp_gmf_pool_register_element(pool, enc_handle, NULL);

    esp_audio_dec_register_default();
    esp_audio_simple_dec_register_default();
    esp_audio_simple_dec_cfg_t es_dec_cfg = DEFAULT_ESP_GMF_AUDIO_DEC_CONFIG();
    esp_gmf_element_handle_t es_hd = NULL;
    esp_gmf_audio_dec_init(&es_dec_cfg, &es_hd);
    esp_gmf_pool_register_element(pool, es_hd, NULL);
}

void pool_unregister_audio_codecs()
{
    esp_audio_enc_unregister_default();
    esp_audio_dec_unregister_default();
    esp_audio_simple_dec_unregister_default();
}

void pool_register_audio_effects(esp_gmf_pool_handle_t pool)
{
    esp_ae_alc_cfg_t alc_cfg = DEFAULT_ESP_GMF_ALC_CONFIG();
    esp_gmf_element_handle_t alc_hd = NULL;
    esp_gmf_alc_init(&alc_cfg, &alc_hd);
    esp_gmf_pool_register_element(pool, alc_hd, NULL);

    esp_ae_eq_cfg_t eq_cfg = DEFAULT_ESP_GMF_EQ_CONFIG();
    esp_gmf_element_handle_t eq_hd = NULL;
    esp_gmf_eq_init(&eq_cfg, &eq_hd);
    esp_gmf_pool_register_element(pool, eq_hd, NULL);

    esp_ae_ch_cvt_cfg_t ch_cvt_cfg = DEFAULT_ESP_GMF_CH_CVT_CONFIG();
    esp_gmf_element_handle_t ch_hd = NULL;
    ch_cvt_cfg.dest_ch = 2;
    esp_gmf_ch_cvt_init(&ch_cvt_cfg, &ch_hd);
    esp_gmf_pool_register_element(pool, ch_hd, NULL);

    esp_ae_bit_cvt_cfg_t bit_cvt_cfg = DEFAULT_ESP_GMF_BIT_CVT_CONFIG();
    bit_cvt_cfg.dest_bits = 16;
    esp_gmf_element_handle_t bit_hd = NULL;
    esp_gmf_bit_cvt_init(&bit_cvt_cfg, &bit_hd);
    esp_gmf_pool_register_element(pool, bit_hd, NULL);

    esp_ae_rate_cvt_cfg_t rate_cvt_cfg = DEFAULT_ESP_GMF_RATE_CVT_CONFIG();
    rate_cvt_cfg.dest_rate = 48000;
    esp_gmf_element_handle_t rate_hd = NULL;
    esp_gmf_rate_cvt_init(&rate_cvt_cfg, &rate_hd);
    esp_gmf_pool_register_element(pool, rate_hd, NULL);

    esp_ae_fade_cfg_t fade_cfg = DEFAULT_ESP_GMF_FADE_CONFIG();
    esp_gmf_element_handle_t fade_hd = NULL;
    esp_gmf_fade_init(&fade_cfg, &fade_hd);
    esp_gmf_pool_register_element(pool, fade_hd, NULL);

    esp_ae_sonic_cfg_t sonic_cfg = DEFAULT_ESP_GMF_SONIC_CONFIG();
    esp_gmf_element_handle_t sonic_hd = NULL;
    esp_gmf_sonic_init(&sonic_cfg, &sonic_hd);
    esp_gmf_pool_register_element(pool, sonic_hd, NULL);

    esp_gmf_deinterleave_cfg deinterleave_cfg = DEFAULT_ESP_GMF_DEINTERLEAVE_CONFIG();
    esp_gmf_element_handle_t deinterleave_hd = NULL;
    esp_gmf_deinterleave_init(&deinterleave_cfg, &deinterleave_hd);
    esp_gmf_pool_register_element(pool, deinterleave_hd, NULL);

    esp_gmf_interleave_cfg interleave_cfg = DEFAULT_ESP_GMF_INTERLEAVE_CONFIG();
    esp_gmf_element_handle_t interleave_hd = NULL;
    esp_gmf_interleave_init(&interleave_cfg, &interleave_hd);
    esp_gmf_pool_register_element(pool, interleave_hd, NULL);

    esp_ae_mixer_cfg_t mixer_cfg = {0};
    mixer_cfg.sample_rate = 48000;
    mixer_cfg.channel = 2;
    mixer_cfg.bits_per_sample = 16;
    mixer_cfg.src_num = 2;
    esp_ae_mixer_info_t source_info[2];
    esp_ae_mixer_info_t mixer_info1 = {
        .weight1 = 1,
        .weight2 = 0.5,
        .transit_time = 500,
    };
    esp_ae_mixer_info_t mixer_info2 = {
        .weight1 = 0.0,
        .weight2 = 0.5,
        .transit_time = 500,
    };
    source_info[0] = mixer_info1;
    source_info[1] = mixer_info2;
    mixer_cfg.src_info = source_info;
    esp_gmf_element_handle_t mixer_hd = NULL;
    esp_gmf_mixer_init(&mixer_cfg, &mixer_hd);
    esp_gmf_pool_register_element(pool, mixer_hd, NULL);
}

void pool_register_all(esp_gmf_pool_handle_t pool, void *play_dev, void *codec_dev)
{
    pool_register_audio_codecs(pool);
    pool_register_audio_effects(pool);
    pool_register_io(pool);
    pool_register_codec_dev_io(pool, play_dev, codec_dev);
}
