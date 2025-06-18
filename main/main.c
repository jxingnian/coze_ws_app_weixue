/*
 * @Author: xingnian j_xingnian@163.com
 * @Date: 2025-06-14 19:45:31
 * @LastEditors: 星年
 * @LastEditTime: 2025-06-18 17:09:51
 * @FilePath: \coze_ws_app_weixue\main\main.c
 * @Description: 
 * 
 * Copyright (c) 2025 by ${git_name_email}, All Rights Reserved. 
 */
/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_err.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_heap_caps.h"  // 添加堆内存管理头文件

#include "protocol_examples_common.h"
#include "esp_coze_chat.h"
#include "coze_chat_app.h"
#include "audio_processor.h"

#include <stdio.h>

#include "freertos/FreeRTOS.h"  // FreeRTOS核心头文件
#include "freertos/task.h"      // FreeRTOS任务管理
#include "driver/gpio.h"        // GPIO控制
#include "driver/i2c_master.h"  // I2C主机驱动
#include "driver/spi_master.h"  // SPI主机驱动
#include "esp_timer.h"          // ESP32定时器
#include "esp_lcd_panel_io.h"   // LCD面板IO接口
#include "esp_lcd_panel_vendor.h" // LCD厂商特定功能
#include "esp_lcd_panel_ops.h"  // LCD面板操作接口

#include "lvgl.h"               // LVGL图形库
#include "lv_demos.h"           // LVGL演示程序
#include "esp_lcd_sh8601.h"     // SH8601 LCD驱动
#include "esp_lcd_touch_ft5x06.h" // FT5x06触摸屏驱动

#include "esp_io_expander_tca9554.h" // TCA9554 IO扩展器
#include "ui.h"                 // 用户界面

static const char *TAG = "COZE_CHAT_WS";

static SemaphoreHandle_t lvgl_mux = NULL;  // LVGL互斥锁

#define LCD_HOST SPI2_HOST      // LCD使用的SPI主机
#define TOUCH_HOST I2C_NUM_0    // 触摸屏使用的I2C主机

// 根据LVGL颜色深度配置LCD像素位深
#if CONFIG_LV_COLOR_DEPTH == 32
#define LCD_BIT_PER_PIXEL (24)
#elif CONFIG_LV_COLOR_DEPTH == 16
#define LCD_BIT_PER_PIXEL (16)
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////// 请根据您的LCD规格更新以下配置 //////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define EXAMPLE_LCD_BK_LIGHT_ON_LEVEL 1     // 背光开启电平
#define EXAMPLE_LCD_BK_LIGHT_OFF_LEVEL !EXAMPLE_LCD_BK_LIGHT_ON_LEVEL  // 背光关闭电平
#define EXAMPLE_PIN_NUM_LCD_CS (GPIO_NUM_12)  // LCD片选引脚
#define EXAMPLE_PIN_NUM_LCD_PCLK (GPIO_NUM_11)  // LCD时钟引脚
#define EXAMPLE_PIN_NUM_LCD_DATA0 (GPIO_NUM_4)  // LCD数据线0
#define EXAMPLE_PIN_NUM_LCD_DATA1 (GPIO_NUM_5)  // LCD数据线1
#define EXAMPLE_PIN_NUM_LCD_DATA2 (GPIO_NUM_6)  // LCD数据线2
#define EXAMPLE_PIN_NUM_LCD_DATA3 (GPIO_NUM_7)  // LCD数据线3
#define EXAMPLE_PIN_NUM_LCD_RST (-1)  // LCD复位引脚，-1表示不使用
#define EXAMPLE_PIN_NUM_BK_LIGHT (-1)  // 背光控制引脚，-1表示不使用

// 水平和垂直方向的像素数
#define EXAMPLE_LCD_H_RES 368  // 水平分辨率
#define EXAMPLE_LCD_V_RES 448  // 垂直分辨率

#define EXAMPLE_USE_TOUCH 1    // 是否使用触摸功能

#if EXAMPLE_USE_TOUCH
#define EXAMPLE_PIN_NUM_TOUCH_SCL (GPIO_NUM_14)  // 触摸屏I2C时钟线
#define EXAMPLE_PIN_NUM_TOUCH_SDA (GPIO_NUM_15)  // 触摸屏I2C数据线
#define EXAMPLE_PIN_NUM_TOUCH_RST (-1)  // 触摸屏复位引脚，-1表示不使用
#define EXAMPLE_PIN_NUM_TOUCH_INT (GPIO_NUM_21)  // 触摸屏中断引脚

esp_lcd_touch_handle_t tp = NULL;  // 触摸屏控制句柄
#endif

#define EXAMPLE_LVGL_BUF_HEIGHT (EXAMPLE_LCD_V_RES / 8)  // LVGL缓冲区高度
#define EXAMPLE_LVGL_TICK_PERIOD_MS 2  // LVGL定时器周期(毫秒)
#define EXAMPLE_LVGL_TASK_MAX_DELAY_MS 500  // LVGL任务最大延迟
#define EXAMPLE_LVGL_TASK_MIN_DELAY_MS 1  // LVGL任务最小延迟
#define EXAMPLE_LVGL_TASK_STACK_SIZE (4 * 1024)  // LVGL任务栈大小
#define EXAMPLE_LVGL_TASK_PRIORITY 2  // LVGL任务优先级


// LCD初始化命令序列
static const sh8601_lcd_init_cmd_t lcd_init_cmds[] = {
    {0x11, (uint8_t[]){0x00}, 0, 120},  // 退出睡眠模式，延时120ms
    {0x44, (uint8_t[]){0x01, 0xD1}, 2, 0},  // 设置垂直滚动区域
    {0x35, (uint8_t[]){0x00}, 1, 0},  // 设置撕裂效应
    {0x53, (uint8_t[]){0x20}, 1, 10},  // 写入控制显示
    {0x2A, (uint8_t[]){0x00, 0x00, 0x01, 0x6F}, 4, 0},  // 设置列地址
    {0x2B, (uint8_t[]){0x00, 0x00, 0x01, 0xBF}, 4, 0},  // 设置页地址
    {0x51, (uint8_t[]){0x00}, 1, 10},  // 写入显示亮度
    {0x29, (uint8_t[]){0x00}, 0, 10},  // 开启显示
    {0x51, (uint8_t[]){0xFF}, 1, 0},  // 设置最大亮度
};

/**
 * @brief LCD面板刷新完成通知回调
 * 
 * @param panel_io LCD面板IO句柄
 * @param edata 事件数据
 * @param user_ctx 用户上下文
 * @return true 继续处理事件
 * @return false 停止处理事件
 */
static bool example_notify_lvgl_flush_ready(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
{
    lv_disp_drv_t *disp_driver = (lv_disp_drv_t *)user_ctx;
    lv_disp_flush_ready(disp_driver);
    return false;
}

/**
 * @brief LVGL刷新回调函数
 * 
 * @param drv 显示驱动
 * @param area 刷新区域
 * @param color_map 颜色数据
 */
static void example_lvgl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map)
{
    esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t)drv->user_data;
    const int offsetx1 = area->x1;
    const int offsetx2 = area->x2;
    const int offsety1 = area->y1;
    const int offsety2 = area->y2;

#if LCD_BIT_PER_PIXEL == 24
    // 24位色彩需要特殊处理RGB格式
    uint8_t *to = (uint8_t *)color_map;
    uint8_t temp = 0;
    uint16_t pixel_num = (offsetx2 - offsetx1 + 1) * (offsety2 - offsety1 + 1);

    // 特殊处理第一个像素
    temp = color_map[0].ch.blue;
    *to++ = color_map[0].ch.red;
    *to++ = color_map[0].ch.green;
    *to++ = temp;
    // 正常处理其他像素
    for (int i = 1; i < pixel_num; i++)
    {
        *to++ = color_map[i].ch.red;
        *to++ = color_map[i].ch.green;
        *to++ = color_map[i].ch.blue;
    }
#endif

    // 将缓冲区内容复制到显示屏的特定区域
    esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, color_map);
}

/**
 * @brief 当LVGL中旋转屏幕时，旋转显示和触摸
 * 
 * @param drv 显示驱动
 */
static void example_lvgl_update_cb(lv_disp_drv_t *drv)
{
    esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t)drv->user_data;

    switch (drv->rotated)
    {
    case LV_DISP_ROT_NONE:
        // 旋转LCD显示 - 无旋转
        esp_lcd_panel_swap_xy(panel_handle, false);
        esp_lcd_panel_mirror(panel_handle, true, false);
        break;
    case LV_DISP_ROT_90:
        // 旋转LCD显示 - 90度
        esp_lcd_panel_swap_xy(panel_handle, true);
        esp_lcd_panel_mirror(panel_handle, true, true);
        break;
    case LV_DISP_ROT_180:
        // 旋转LCD显示 - 180度
        esp_lcd_panel_swap_xy(panel_handle, false);
        esp_lcd_panel_mirror(panel_handle, false, true);
        break;
    case LV_DISP_ROT_270:
        // 旋转LCD显示 - 270度
        esp_lcd_panel_swap_xy(panel_handle, true);
        esp_lcd_panel_mirror(panel_handle, false, false);
        break;
    }
}

/**
 * @brief LVGL坐标舍入回调函数
 * 
 * @param disp_drv 显示驱动
 * @param area 区域
 */
void example_lvgl_rounder_cb(struct _lv_disp_drv_t *disp_drv, lv_area_t *area)
{
    uint16_t x1 = area->x1;
    uint16_t x2 = area->x2;

    uint16_t y1 = area->y1;
    uint16_t y2 = area->y2;

    // 将坐标起始点向下舍入到最近的2M数
    area->x1 = (x1 >> 1) << 1;
    area->y1 = (y1 >> 1) << 1;
    // 将坐标结束点向上舍入到最近的2N+1数
    area->x2 = ((x2 >> 1) << 1) + 1;
    area->y2 = ((y2 >> 1) << 1) + 1;
}

#if EXAMPLE_USE_TOUCH
/**
 * @brief LVGL触摸输入回调函数
 * 
 * @param drv 输入设备驱动
 * @param data 输入数据
 */
static void example_lvgl_touch_cb(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
    esp_lcd_touch_handle_t tp = (esp_lcd_touch_handle_t)drv->user_data;
    assert(tp);

    uint16_t tp_x;
    uint16_t tp_y;
    uint8_t tp_cnt = 0;
    /* 从触摸控制器读取数据到内存 */
    esp_lcd_touch_read_data(tp);
    // ESP_LOGI(TAG, "Touch data read from controller");
    /* 从触摸控制器读取坐标 */
    bool tp_pressed = esp_lcd_touch_get_coordinates(tp, &tp_x, &tp_y, NULL, &tp_cnt, 1);
    // ESP_LOGI(TAG, "Touch get_coordinates: pressed=%d, count=%d, x=%d, y=%d", tp_pressed, tp_cnt, tp_x, tp_y);
    if (tp_pressed && tp_cnt > 0)
    {
        data->point.x = tp_x;
        data->point.y = tp_y;
        data->state = LV_INDEV_STATE_PRESSED;
        ESP_LOGD(TAG, "Touch position: %d,%d", tp_x, tp_y);
        ESP_LOGI(TAG, "Touch pressed at (%d, %d)", tp_x, tp_y);
    }
    else
    {
        data->state = LV_INDEV_STATE_RELEASED;
        // ESP_LOGI(TAG, "Touch released");
    }
}
#endif

/**
 * @brief 增加LVGL计时器计数
 * 
 * @param arg 参数
 */
static void example_increase_lvgl_tick(void *arg)
{
    /* 告诉LVGL已经过去了多少毫秒 */
    lv_tick_inc(EXAMPLE_LVGL_TICK_PERIOD_MS);
}

/**
 * @brief 获取LVGL互斥锁
 * 
 * @param timeout_ms 超时时间(毫秒)
 * @return true 成功获取锁
 * @return false 获取锁失败
 */
static bool example_lvgl_lock(int timeout_ms)
{
    assert(lvgl_mux && "bsp_display_start must be called first");

    const TickType_t timeout_ticks = (timeout_ms == -1) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    return xSemaphoreTake(lvgl_mux, timeout_ticks) == pdTRUE;
}

/**
 * @brief 释放LVGL互斥锁
 */
static void example_lvgl_unlock(void)
{
    assert(lvgl_mux && "bsp_display_start must be called first");
    xSemaphoreGive(lvgl_mux);
}

/**
 * @brief LVGL任务处理函数
 * 
 * @param arg 参数
 */
static void example_lvgl_port_task(void *arg)
{
    ESP_LOGI(TAG, "Starting LVGL task");
    uint32_t task_delay_ms = EXAMPLE_LVGL_TASK_MAX_DELAY_MS;
    while (1)
    {
        // 获取互斥锁，因为LVGL API不是线程安全的
        if (example_lvgl_lock(-1))
        {
            task_delay_ms = lv_timer_handler();
            // 释放互斥锁
            example_lvgl_unlock();
        }
        if (task_delay_ms > EXAMPLE_LVGL_TASK_MAX_DELAY_MS)
        {
            task_delay_ms = EXAMPLE_LVGL_TASK_MAX_DELAY_MS;
        }
        else if (task_delay_ms < EXAMPLE_LVGL_TASK_MIN_DELAY_MS)
        {
            task_delay_ms = EXAMPLE_LVGL_TASK_MIN_DELAY_MS;
        }
        vTaskDelay(pdMS_TO_TICKS(task_delay_ms));
    }
}

static esp_err_t spiffs_filesystem_init(void)
{
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = "spiffs_data",
        .max_files = 5,
        .format_if_mount_failed = false
      };
  
      esp_err_t ret = esp_vfs_spiffs_register(&conf);
  
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
            return ESP_FAIL;
        } else {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
            return ESP_FAIL;
        }
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info(NULL, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }
    return ESP_OK;
}


// 定时任务：每隔2秒显示一行新文字
static void chat_task(void *pvParameters)
{
    int i = 0;
    char buf[64];
    while (1) {
        snprintf(buf, sizeof(buf), "定时消息 %d", ++i);
        vTaskDelay(pdMS_TO_TICKS(2000));

        // 打印堆内存、psram内存、内部内存
        ESP_LOGI(TAG, "Heap free: %u bytes", heap_caps_get_free_size(MALLOC_CAP_DEFAULT));
        ESP_LOGI(TAG, "Internal RAM free: %u bytes", heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
        #if CONFIG_SPIRAM
            ESP_LOGI(TAG, "PSRAM free: %u bytes", heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
        #endif

        if (example_lvgl_lock(-1))
        {
            show_text(buf);
            // 释放互斥锁
            example_lvgl_unlock();
        }
    }
}

    i2c_master_bus_handle_t i2c_bus_handle = NULL;
    extern i2c_master_bus_handle_t   i2c_handle0;
void app_main(void)
{

    esp_log_level_set("*", ESP_LOG_INFO);

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    spiffs_filesystem_init();

    // esp_log_level_set("lcd_panel.io.i2c", ESP_LOG_NONE);
    // esp_log_level_set("FT5x06", ESP_LOG_NONE);
    static lv_disp_draw_buf_t disp_buf; // 包含内部图形缓冲区，称为绘制缓冲区
    static lv_disp_drv_t disp_drv;      // 包含回调函数

    ESP_LOGI(TAG, "Initialize I2C bus");
    i2c_master_bus_config_t i2c_bus_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = TOUCH_HOST,
        .sda_io_num = EXAMPLE_PIN_NUM_TOUCH_SDA,
        .scl_io_num = EXAMPLE_PIN_NUM_TOUCH_SCL,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_config, &i2c_bus_handle));

    // 初始化IO扩展器
    esp_io_expander_handle_t io_expander = NULL;
    ESP_ERROR_CHECK(esp_io_expander_new_i2c_tca9554(i2c_bus_handle, ESP_IO_EXPANDER_I2C_TCA9554_ADDRESS_000, &io_expander));

    // 配置IO扩展器引脚方向和电平
    esp_io_expander_set_dir(io_expander, IO_EXPANDER_PIN_NUM_0 | IO_EXPANDER_PIN_NUM_1 | IO_EXPANDER_PIN_NUM_2, IO_EXPANDER_OUTPUT);
    esp_io_expander_set_level(io_expander, IO_EXPANDER_PIN_NUM_0, 0);
    esp_io_expander_set_level(io_expander, IO_EXPANDER_PIN_NUM_1, 0);
    esp_io_expander_set_level(io_expander, IO_EXPANDER_PIN_NUM_2, 0);
    vTaskDelay(pdMS_TO_TICKS(200));
    esp_io_expander_set_level(io_expander, IO_EXPANDER_PIN_NUM_0, 1);
    esp_io_expander_set_level(io_expander, IO_EXPANDER_PIN_NUM_1, 1);
    esp_io_expander_set_level(io_expander, IO_EXPANDER_PIN_NUM_2, 1);
    i2c_handle0 = i2c_bus_handle;

#if EXAMPLE_PIN_NUM_BK_LIGHT >= 0
    ESP_LOGI(TAG, "Turn off LCD backlight");
    gpio_config_t bk_gpio_config = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << EXAMPLE_PIN_NUM_BK_LIGHT};
    ESP_ERROR_CHECK(gpio_config(&bk_gpio_config));
#endif

    ESP_LOGI(TAG, "Initialize SPI bus");
    // 配置SPI总线，使用QSPI模式
    const spi_bus_config_t buscfg = SH8601_PANEL_BUS_QSPI_CONFIG(EXAMPLE_PIN_NUM_LCD_PCLK,
                                                                 EXAMPLE_PIN_NUM_LCD_DATA0,
                                                                 EXAMPLE_PIN_NUM_LCD_DATA1,
                                                                 EXAMPLE_PIN_NUM_LCD_DATA2,
                                                                 EXAMPLE_PIN_NUM_LCD_DATA3,
                                                                 EXAMPLE_LCD_H_RES * EXAMPLE_LCD_V_RES * LCD_BIT_PER_PIXEL / 8);
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

    ESP_LOGI(TAG, "Install panel IO");
    esp_lcd_panel_io_handle_t io_handle = NULL;
    // 配置LCD面板IO，使用QSPI模式
    const esp_lcd_panel_io_spi_config_t io_config = SH8601_PANEL_IO_QSPI_CONFIG(EXAMPLE_PIN_NUM_LCD_CS,
                                                                                example_notify_lvgl_flush_ready,
                                                                                &disp_drv);
    // SH8601 LCD厂商特定配置
    sh8601_vendor_config_t vendor_config = {
        .init_cmds = lcd_init_cmds,
        .init_cmds_size = sizeof(lcd_init_cmds) / sizeof(lcd_init_cmds[0]),
        .flags = {
            .use_qspi_interface = 1,
        },
    };
    // 将LCD连接到SPI总线
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &io_handle));

    esp_lcd_panel_handle_t panel_handle = NULL;
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = EXAMPLE_PIN_NUM_LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = LCD_BIT_PER_PIXEL,
        .vendor_config = &vendor_config,
    };
    ESP_LOGI(TAG, "Install SH8601 panel driver");
    ESP_ERROR_CHECK(esp_lcd_new_panel_sh8601(io_handle, &panel_config, &panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    // 用户可以在打开屏幕或背光之前将预定义的图案刷新到屏幕上
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

#if EXAMPLE_USE_TOUCH
    // 初始化触摸屏
    esp_lcd_panel_io_handle_t tp_io_handle = NULL;

esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_FT5x06_CONFIG();
tp_io_config.scl_speed_hz = 16 * 1000;
ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(i2c_bus_handle, &tp_io_config, &tp_io_handle));


    const esp_lcd_touch_config_t tp_cfg = {
        .x_max = EXAMPLE_LCD_H_RES,
        .y_max = EXAMPLE_LCD_V_RES,
        .rst_gpio_num = EXAMPLE_PIN_NUM_TOUCH_RST,
        .int_gpio_num = EXAMPLE_PIN_NUM_TOUCH_INT,
        .levels = {
            .reset = 0,
            .interrupt = 0,
        },
        .flags = {
            .swap_xy = 0,
            .mirror_x = 0,
            .mirror_y = 0,
        },
    };

    ESP_LOGI(TAG, "Initialize touch controller");
    ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_ft5x06(tp_io_handle, &tp_cfg, &tp));
#endif

#if EXAMPLE_PIN_NUM_BK_LIGHT >= 0
    ESP_LOGI(TAG, "Turn on LCD backlight");
    gpio_set_level(EXAMPLE_PIN_NUM_BK_LIGHT, EXAMPLE_LCD_BK_LIGHT_ON_LEVEL);
#endif

    ESP_LOGI(TAG, "Initialize LVGL library");
    lv_init();
    // 将LVGL绘制缓冲区分配到堆区
    lv_color_t *buf1 = (lv_color_t *)malloc(EXAMPLE_LCD_H_RES * EXAMPLE_LVGL_BUF_HEIGHT * sizeof(lv_color_t));
    assert(buf1);
    lv_color_t *buf2 = (lv_color_t *)malloc(EXAMPLE_LCD_H_RES * EXAMPLE_LVGL_BUF_HEIGHT * sizeof(lv_color_t));
    assert(buf2);
    // 初始化LVGL绘制缓冲区
    lv_disp_draw_buf_init(&disp_buf, buf1, buf2, EXAMPLE_LCD_H_RES * EXAMPLE_LVGL_BUF_HEIGHT);

    ESP_LOGI(TAG, "Register display driver to LVGL");
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = EXAMPLE_LCD_H_RES;
    disp_drv.ver_res = EXAMPLE_LCD_V_RES;
    disp_drv.flush_cb = example_lvgl_flush_cb;
    disp_drv.rounder_cb = example_lvgl_rounder_cb;
    disp_drv.drv_update_cb = example_lvgl_update_cb;
    disp_drv.draw_buf = &disp_buf;
    disp_drv.user_data = panel_handle;
    lv_disp_t *disp = lv_disp_drv_register(&disp_drv);

    ESP_LOGI(TAG, "Install LVGL tick timer");
    // LVGL的定时器接口(使用esp_timer生成2ms周期事件)
    const esp_timer_create_args_t lvgl_tick_timer_args = {
        .callback = &example_increase_lvgl_tick,
        .name = "lvgl_tick"};
    esp_timer_handle_t lvgl_tick_timer = NULL;
    ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, EXAMPLE_LVGL_TICK_PERIOD_MS * 1000));

#if EXAMPLE_USE_TOUCH
    // 注册触摸输入设备
    static lv_indev_drv_t indev_drv; // 输入设备驱动(触摸)
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.disp = disp;
    indev_drv.read_cb = example_lvgl_touch_cb;
    indev_drv.user_data = tp;
    lv_indev_drv_register(&indev_drv);
#endif

    // 创建LVGL互斥锁
    lvgl_mux = xSemaphoreCreateMutex();
    assert(lvgl_mux);
    xTaskCreate(example_lvgl_port_task, "LVGL", EXAMPLE_LVGL_TASK_STACK_SIZE, NULL, EXAMPLE_LVGL_TASK_PRIORITY, NULL);

    ESP_LOGI(TAG, "Display LVGL demos");
    // 获取互斥锁，因为LVGL API不是线程安全的
    if (example_lvgl_lock(-1))
    {
        ui_init();  // 初始化用户界面

        ui_events_init();
        // lv_demo_widgets(); /* 小部件示例 */
        // lv_demo_music(); /* 现代化、类似智能手机的音乐播放器演示 */
        // lv_demo_stress();       /* LVGL压力测试 */
        // lv_demo_benchmark(); /* 用于测量LVGL性能或比较不同设置的演示 */

        // 释放互斥锁
        example_lvgl_unlock();
    }
    
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    ESP_ERROR_CHECK(example_connect());

    coze_chat_app_init();

    // 启动定时任务，每2秒增加一行文字
    xTaskCreate(chat_task, "chat_task", 4096, NULL, 5, NULL);
}
