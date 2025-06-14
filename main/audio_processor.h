/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  音频录制事件回调函数的类型定义
 *
 * @param[in]  event  指向事件数据的指针，可以包含有关录音机状态的信息
 * @param[in]  ctx    用户定义的上下文指针，在注册回调时传递
 */
typedef void (*recorder_event_callback_t)(void *event, void *ctx);

/**
 * @brief  初始化音频管理器模块
 *
 * @return
 *       - ESP_OK  成功
 *       - Other   失败时返回相应的esp_err_t错误代码
 */
esp_err_t audio_manager_init();

/**
 * @brief  反初始化音频管理器组件
 *
 * @return
 *       - ESP_OK  成功
 *       - Other   失败时返回相应的esp_err_t错误代码
 */
esp_err_t audio_manager_deinit();

/**
 * @brief  打开音频录制器并注册事件回调
 *
 * @param[in]  cb   指向事件回调函数的指针，将在录音机事件上调用
 * @param[in]  ctx  传递给回调函数的用户定义上下文指针
 *
 * @return
 *       - ESP_OK  成功
 *       - Other   失败时返回相应的esp_err_t错误代码
 */
esp_err_t audio_recorder_open(recorder_event_callback_t cb, void *ctx);

/**
 * @brief  关闭音频录制器
 *
 * @return
 *       - ESP_OK  成功
 *       - Other   失败时返回相应的esp_err_t错误代码
 */
esp_err_t audio_recorder_close(void);

/**
 * @brief  从录音机读取音频数据
 *
 * @param[out]  data       指向将存储音频数据的缓冲区的指针
 * @param[in]   data_size  存储音频数据的缓冲区大小
 *
 * @return
 *       - ESP_OK  成功
 *       - Other   失败时返回相应的esp_err_t错误代码
 */
esp_err_t audio_recorder_read_data(uint8_t *data, int data_size);

/**
 * @brief  打开音频播放系统
 *
 * @return
 *       - ESP_OK  成功
 *       - Other   失败时返回相应的esp_err_t错误代码
 */
esp_err_t audio_playback_open();

/**
 * @brief  关闭音频播放模块
 *
 * @return
 *       - ESP_OK  成功
 *       - Other   失败时返回相应的esp_err_t错误代码
 */
esp_err_t audio_playback_close(void);

/**
 * @brief  启动音频播放操作
 *
 * @return
 *       - ESP_OK  成功
 *       - Other   失败时返回相应的esp_err_t错误代码
 */
esp_err_t audio_playback_run(void);

/**
 * @brief  停止正在进行的音频播放
 *
 * @return
 *       - ESP_OK  成功停止音频播放
 *       - Other   失败时返回相应的esp_err_t错误代码
 */
esp_err_t audio_playback_stop(void);

/**
 * @brief  向播放系统提供音频数据
 *
 * @param[in]  data       指向要输入到播放系统的音频数据的指针
 * @param[in]  data_size  要播放的音频数据的大小
 *
 * @return
 *       - ESP_OK  成功
 *       - Other   失败时返回相应的esp_err_t错误代码
 */
esp_err_t audio_playback_feed_data(uint8_t *data, int data_size);

/**
 * @brief  打开音频提示系统
 *
 * @return
 *       - ESP_OK  成功
 *       - Other   失败时返回相应的esp_err_t错误代码
 */
esp_err_t audio_prompt_open(void);

/**
 * @brief  关闭音频提示功能
 *
 * @return
 *       - ESP_OK  成功
 *       - Other   失败时返回相应的esp_err_t错误代码
 */
esp_err_t audio_prompt_close(void);

/**
 * @brief  从指定URL播放音频提示
 *
 * @param[in]  url  指向要播放的音频提示文件的URL
 *
 * @return
 *       - ESP_OK  成功
 *       - Other   失败时返回相应的esp_err_t错误代码
 */
esp_err_t audio_prompt_play(const char *url);

/**
 * @brief  停止当前播放的音频提示
 *
 * @return
 *       - ESP_OK  成功
 *       - Other   失败时返回相应的esp_err_t错误代码
 */
esp_err_t audio_prompt_stop(void);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
