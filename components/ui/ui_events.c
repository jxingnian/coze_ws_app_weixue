/*
 * @Author: jixingnian@gmail.com
 * @Date: 2025-06-12 15:42:08
 * @LastEditTime: 2025-06-19 19:50:54
 * @LastEditors: 星年 && j_xingnian@163.com
 * @Description: 
 * @FilePath: \coze_ws_app_weixue\components\ui\ui_events.c
 * 遇事不决，可问春风
 */
#include "ui.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_heap_caps.h"  // 添加堆内存管理头文件

static const char *TAG = "ui_events";

#define CHAT_HEIGHT 250
#define CHAT_WIDTH 300
#define CHAT_X 0
#define CHAT_Y -15
#define CHAT_MARGIN 20
#define MAX_CHAT_LINES 30  // 最大消息行数限制
#define MAX_SUBTITLE_LEN 4096  // 字幕最大长度

/* 电池电量低 */
#define BATTERY_LOW "1"
/* 电池电量中 */
#define BATTERY_MID "2"
/* 电池电量高 */
#define BATTERY_HIGH "3"
/* 网络WIFI */
#define NETWORK_WIFI "4"
/* 网络有线 */
#define NETWORK_CABLE "5"
/* 网络4G */
#define NETWORK_4G "6"
/* 音频播放 */
#define STATUS_PLAY "7"
/* 音频待机 */
#define STATUS_WAIT "8" 
/* 音频MIC */
#define STATUS_MIC "9"

lv_obj_t * ui_chat;
lv_obj_t * chat_messages[MAX_CHAT_LINES] = {NULL};  // 存储消息对象的数组

int chat_count = 0;

// 字幕缓存
static char subtitle_buffer[MAX_SUBTITLE_LEN] = {0};
static int subtitle_len = 0;

SemaphoreHandle_t lvgl_mux = NULL;  // LVGL互斥锁

/**
 * @brief 获取LVGL互斥锁
 * 
 * @param timeout_ms 超时时间(毫秒)
 * @return true 成功获取锁
 * @return false 获取锁失败
 */
 bool example_lvgl_lock(int timeout_ms)
{
    assert(lvgl_mux && "bsp_display_start must be called first");

    const TickType_t timeout_ticks = (timeout_ms == -1) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    return xSemaphoreTake(lvgl_mux, timeout_ticks) == pdTRUE;
}


/**
 * @brief 释放LVGL互斥锁
 */
 void example_lvgl_unlock(void)
{
    assert(lvgl_mux && "bsp_display_start must be called first");
    xSemaphoreGive(lvgl_mux);
}
static uint8_t subtitle_flag = 0;
static uint8_t subtitle_count = 0;

// 定时任务函数，每500ms执行一次，内容为空
static void ui_events_timer_task(void *pvParameters)
{
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(200));
        if(subtitle_flag == 1){
            vTaskDelay(pdMS_TO_TICKS(1000));
            show_and_clear_subtitle();
        }
    }
}

/**
 * @brief 添加字幕文本并在适当时候显示
 * 
 * @param text 字幕文本片段
 */
void add_subtitle(char *text) {
    // 将接收到的新文本追加到字幕缓冲区中
    strncat(subtitle_buffer, text, MAX_SUBTITLE_LEN - subtitle_len - 1);
    subtitle_len = strlen(subtitle_buffer);
    if(subtitle_flag == 0){
        subtitle_flag = 1;
    }
}

/**
 * @brief 显示并清空字幕缓冲区
 * 
 * 显示当前字幕缓冲区中的所有文本，然后清空缓冲区
 */
void show_and_clear_subtitle(void) {
    if (subtitle_len > 0) {
        // 显示缓冲区中的文本
        show_text(subtitle_buffer);
        
        // 清空缓冲区
        memset(subtitle_buffer, 0, MAX_SUBTITLE_LEN);
        subtitle_len = 0;
    }
}


void new_chat(lv_obj_t *ui_chat, char *text){
    // 检查是否达到最大消息数量
    if (chat_count >= MAX_CHAT_LINES) {
        // 删除最旧的消息（第一条）
        if (chat_messages[0] != NULL) {
            lv_obj_del(chat_messages[0]);
            
            // 将所有消息向前移动一位
            for (int i = 0; i < MAX_CHAT_LINES - 1; i++) {
                chat_messages[i] = chat_messages[i + 1];
                
                // 如果消息存在，更新其Y坐标
                if (chat_messages[i] != NULL) {
                    lv_obj_set_y(chat_messages[i], CHAT_Y + i * (CHAT_HEIGHT + CHAT_MARGIN));
                }
            }
            
            // 最后一个位置设为NULL
            chat_messages[MAX_CHAT_LINES - 1] = NULL;
            
            // 不增加chat_count，因为总数保持不变
        }
    } else {
        // 未达到最大数量，增加计数
        chat_count++;
    }
    
    // 创建新消息
    int msg_index = (chat_count >= MAX_CHAT_LINES) ? (MAX_CHAT_LINES - 1) : (chat_count - 1);
    ui_chat = lv_textarea_create(ui_Panel1);
    
    // 保存到消息数组中
    chat_messages[msg_index] = ui_chat;
    
    // 设置文本区域的宽度为 CHAT_WIDTH
    lv_obj_set_width(ui_chat, CHAT_WIDTH);
    
    // 设置文本区域的高度为 CHAT_HEIGHT
    lv_obj_set_height(ui_chat, CHAT_HEIGHT);
    
    // 设置文本区域的 X 坐标为 CHAT_X
    lv_obj_set_x(ui_chat, CHAT_X);
    
    // 设置文本区域的 Y 坐标
    lv_obj_set_y(ui_chat, CHAT_Y + msg_index * (CHAT_HEIGHT + CHAT_MARGIN));
    
    // 设置文本区域在父容器中的对齐方式为居中
    lv_obj_set_align(ui_chat, LV_ALIGN_CENTER);
    
    // 清除可点击聚焦标志，禁止该文本区域获取焦点（不可编辑，仅显示）
    lv_obj_clear_flag(ui_chat, LV_OBJ_FLAG_CLICK_FOCUSABLE);
    
    // 设置文本区域显示的内容为传入的 text 字符串
    lv_textarea_set_text(ui_chat, text);
    
    // 设置文本区域的字体为自定义字体 ui_font_dinglie32
    lv_obj_set_style_text_font(ui_chat, &ui_font_dinglie32, LV_PART_MAIN | LV_STATE_DEFAULT);
    
    // // 设置文本区域为单行显示，超出部分自动滚动
    // lv_textarea_set_one_line(ui_chat, true);
}


// 显示文字
void show_text(char *text){    
    if (example_lvgl_lock(-1))
    {
        ESP_LOGI(TAG, "show_text: %s", text);
        new_chat(ui_chat, text);
        
        // 计算需要滚动的位置
        int32_t scroll_y = 0;
        if (chat_count >= MAX_CHAT_LINES) {
            scroll_y = (MAX_CHAT_LINES - 1) * (CHAT_HEIGHT + CHAT_MARGIN);
        } else {
            scroll_y = (chat_count - 1) * (CHAT_HEIGHT + CHAT_MARGIN);
        }
        
        // 滚动到最新消息的位置
        lv_obj_scroll_to_y(ui_Panel1, scroll_y, LV_ANIM_OFF);
        // 释放互斥锁
        example_lvgl_unlock();
    }
}


void ui_events_init(void){
    // 创建LVGL互斥锁
    lvgl_mux = xSemaphoreCreateMutex();
    assert(lvgl_mux);
    ESP_LOGI(TAG, "Display LVGL demos");
    // 获取互斥锁，因为LVGL API不是线程安全的
    if (example_lvgl_lock(-1))
    {
  
        ui_init();  // 初始化用户界面   

        // lv_demo_widgets(); /* 小部件示例 */
        // lv_demo_music(); /* 现代化、类似智能手机的音乐播放器演示 */
        // lv_demo_stress();       /* LVGL压力测试 */
        // lv_demo_benchmark(); /* 用于测量LVGL性能或比较不同设置的演示 */


        lv_label_set_text(ui_Time, "13:14");

        lv_label_set_text(ui_status, STATUS_WAIT);
        lv_label_set_text(ui_network, NETWORK_WIFI);
        lv_label_set_text(ui_battery, BATTERY_LOW);
        
        // 设置Panel1为可滚动
        lv_obj_add_flag(ui_Panel1, LV_OBJ_FLAG_SCROLLABLE);
        
        // 设置滚动条模式为自动
        lv_obj_set_scrollbar_mode(ui_Panel1, LV_SCROLLBAR_MODE_AUTO);
        
        // 设置滚动方向仅为垂直方向
        lv_obj_set_scroll_dir(ui_Panel1, LV_DIR_VER);

        // 初始化消息数组
        for (int i = 0; i < MAX_CHAT_LINES; i++) {
            chat_messages[i] = NULL;
        }

        // new_chat(ui_chat, "你好,我是小星");
        // 释放互斥锁
        example_lvgl_unlock();
    }

    // 创建定时任务，每500ms执行一次
    xTaskCreate(ui_events_timer_task, "ui_events_timer_task", 2048, NULL, 5, NULL);
    
}
