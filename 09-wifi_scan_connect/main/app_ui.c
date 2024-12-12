#include "app_ui.h" // 引入应用程序用户界面头文件
#include "esp32_s3_szp.h"// 引入 ESP32 S3 特定的硬件配置头文件
#include "esp_wifi.h" // 引入 ESP32 WiFi 功能头文件
#include "freertos/event_groups.h"// 引入 FreeRTOS 事件组头文件
#include "esp_event.h"// 引入 ESP 事件处理头文件


static const char *TAG = "app_ui";// 日志标签，用于调试信息输出

LV_FONT_DECLARE(font_alipuhui20);// 声明自定义字体

// 声明多个对象用于不同的 UI 页面和组件
lv_obj_t *wifi_scan_page;     // wifi扫描页面 obj
lv_obj_t *wifi_connect_page;  // wifi连接页面 obj
lv_obj_t *wifi_password_page; // wifi密码页面 obj
lv_obj_t *wifi_list;          // wifi列表  list
lv_obj_t *label_wifi_connect; // wifi连接页面label 
lv_obj_t *ta_pass_text;       // 密码输入文本框 textarea
lv_obj_t *roller_num;         // 数字roller
lv_obj_t *roller_letter_low;  // 小写字母roller
lv_obj_t *roller_letter_up;   // 大写字母roller
lv_obj_t *label_wifi_name;    // wifi名称label


#define DEFAULT_SCAN_LIST_SIZE   10                // 最大扫描wifi个数

// wifi事件组
static EventGroupHandle_t s_wifi_event_group;// 定义 WiFi 事件组句柄
// wifi事件
#define WIFI_CONNECTED_BIT    BIT0// WiFi 连接成功标志
#define WIFI_FAIL_BIT         BIT1// WiFi 连接失败标志
#define WIFI_START_BIT        BIT2// WiFi 开始连接标志
// wifi最大重连次数
#define EXAMPLE_ESP_MAXIMUM_RETRY  3 // 最大重连次数

// wifi账号队列
static QueueHandle_t xQueueWifiAccount = NULL;  // 定义 WiFi 账号队列句柄

// 队列要传输的内容结构体
typedef struct {
    char wifi_ssid[32];  // 定义wifi名称
    char wifi_password[64]; // 定义wifi密码         
} wifi_account_t;

// 密码滚轮的遮罩显示效果
static void mask_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);// 获取事件代码
    lv_obj_t * obj = lv_event_get_target(e);// 获取事件目标对象

    static int16_t mask_top_id = -1;  // 顶部遮罩 ID
    static int16_t mask_bottom_id = -1; // 底部遮罩 ID

    if(code == LV_EVENT_COVER_CHECK) {
        lv_event_set_cover_res(e, LV_COVER_RES_MASKED);// 设置遮罩效果
    }
    else if(code == LV_EVENT_DRAW_MAIN_BEGIN) {
        /* 添加遮罩 */
        const lv_font_t * font = lv_obj_get_style_text_font(obj, LV_PART_MAIN);// 获取字体
        lv_coord_t line_space = lv_obj_get_style_text_line_space(obj, LV_PART_MAIN);// 获取行间距
        lv_coord_t font_h = lv_font_get_line_height(font);// 获取字体高度

        lv_area_t roller_coords; // 滚轮坐标区域
        lv_obj_get_coords(obj, &roller_coords);// 获取对象坐标

        lv_area_t rect_area;// 矩形区域
        rect_area.x1 = roller_coords.x1;// 左上角 x 坐标
        rect_area.x2 = roller_coords.x2;// 右下角 x 坐标
        rect_area.y1 = roller_coords.y1;// 左上角 y 坐标
        rect_area.y2 = roller_coords.y1 + (lv_obj_get_height(obj) - font_h - line_space) / 2;// 计算 y2 坐标

        // 创建顶部遮罩
        lv_draw_mask_fade_param_t * fade_mask_top = lv_mem_buf_get(sizeof(lv_draw_mask_fade_param_t));
        lv_draw_mask_fade_init(fade_mask_top, &rect_area, LV_OPA_TRANSP, rect_area.y1, LV_OPA_COVER, rect_area.y2);// 初始化顶部遮罩
        mask_top_id = lv_draw_mask_add(fade_mask_top, NULL);// 添加顶部遮罩

        rect_area.y1 = rect_area.y2 + font_h + line_space - 1;// 更新 y1 坐标
        rect_area.y2 = roller_coords.y2; // 更新 y2 坐标

        // 创建底部遮罩
        lv_draw_mask_fade_param_t * fade_mask_bottom = lv_mem_buf_get(sizeof(lv_draw_mask_fade_param_t));
        lv_draw_mask_fade_init(fade_mask_bottom, &rect_area, LV_OPA_COVER, rect_area.y1, LV_OPA_TRANSP, rect_area.y2);  // 初始化底部遮罩
        mask_bottom_id = lv_draw_mask_add(fade_mask_bottom, NULL);// 添加底部遮罩

    }
    else if(code == LV_EVENT_DRAW_POST_END) {
        // 移除遮罩
        lv_draw_mask_fade_param_t * fade_mask_top = lv_draw_mask_remove_id(mask_top_id);
        lv_draw_mask_fade_param_t * fade_mask_bottom = lv_draw_mask_remove_id(mask_bottom_id);
        lv_draw_mask_free_param(fade_mask_top); // 释放顶部遮罩参数
        lv_draw_mask_free_param(fade_mask_bottom);// 释放底部遮罩参数
        lv_mem_buf_release(fade_mask_top);// 释放顶部遮罩内存
        lv_mem_buf_release(fade_mask_bottom);// 释放底部遮罩内存
        mask_top_id = -1;// 重置顶部遮罩 ID
        mask_bottom_id = -1;// 重置底部遮罩 ID
    }
}

// 数字键处理函数
static void btn_num_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e); // 获取事件代码

    if(code == LV_EVENT_CLICKED) {  // 如果事件为点击
        ESP_LOGI(TAG, "Btn-Num Clicked");// 打印日志
        char buf[2]; // 接收滚轮的值
        lv_roller_get_selected_str(roller_num, buf, sizeof(buf));// 获取滚轮选择的字符串
        lv_textarea_add_text(ta_pass_text, buf);// 将选择的值添加到密码输入框
    }
}

// 小写字母确认键处理函数
static void btn_letter_low_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e); // 获取事件代码

    if(code == LV_EVENT_CLICKED) {// 如果事件为点击
        ESP_LOGI(TAG, "Btn-Letter-Low Clicked");// 打印日志
        char buf[2]; // 接收滚轮的值
        lv_roller_get_selected_str(roller_letter_low, buf, sizeof(buf)); // 获取滚轮选择的字符串
        lv_textarea_add_text(ta_pass_text, buf); // 将选择的值添加到密码输入框
    }
}

// 大写字母确认键处理函数
static void btn_letter_up_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);// 获取事件代码

    if(code == LV_EVENT_CLICKED) {// 如果事件为点击
        ESP_LOGI(TAG, "Btn-Letter-Up Clicked");// 打印日志
        char buf[2]; // 接收滚轮的值
        lv_roller_get_selected_str(roller_letter_up, buf, sizeof(buf));  // 获取滚轮选择的字符串
        lv_textarea_add_text(ta_pass_text, buf);// 将选择的值添加到密码输入框
    }
}

// 连接 WiFi 的函数
static void lv_wifi_connect(void)
{
    lv_obj_del(wifi_password_page); // 删除密码输入界面

    // 创建一个面板对象
    static lv_style_t style;// 定义样式变量
    lv_style_init(&style);// 初始化样式
    lv_style_set_bg_opa( &style, LV_OPA_COVER );// 设置背景透明度
    lv_style_set_border_width(&style, 0); // 设置边框宽度
    lv_style_set_pad_all(&style, 0);// 设置内间距
    lv_style_set_radius(&style, 0);  // 设置圆角半径
    lv_style_set_width(&style, 320);  // 设置宽度
    lv_style_set_height(&style, 240); // 设置高度


    wifi_connect_page = lv_obj_create(lv_scr_act());// 创建 WiFi 连接页面
    lv_obj_add_style(wifi_connect_page, &style, 0);// 添加样式

    // 绘制提示标签 
    label_wifi_connect = lv_label_create(wifi_connect_page);// 创建标签
    lv_label_set_text(label_wifi_connect, "WLAN连接中...");// 设置标签文本
    lv_obj_set_style_text_font(label_wifi_connect, &font_alipuhui20, 0);// 设置字体
    lv_obj_align(label_wifi_connect, LV_ALIGN_CENTER, 0, -50);// 对齐标签
}

//WiFi 连接按钮处理函数
static void btn_connect_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);// 获取事件代码

    if(code == LV_EVENT_CLICKED) {// 如果事件为点击
        ESP_LOGI(TAG, "OK Clicked");// 打印日志
        const char *wifi_ssid = lv_label_get_text(label_wifi_name);// 获取 WiFi 名称
        const char *wifi_password = lv_textarea_get_text(ta_pass_text);// 获取输入的密码
        if(*wifi_password != '\0') // 判断密码是否为空字符串
        {
            wifi_account_t wifi_account;// 创建 WiFi 账号结构体
            strcpy(wifi_account.wifi_ssid, wifi_ssid);// 复制 WiFi 名称
            strcpy(wifi_account.wifi_password, wifi_password);// 复制 WiFi 密码
            ESP_LOGI(TAG, "connected to ap SSID:%s password:%s",
                    wifi_account.wifi_ssid, wifi_account.wifi_password);// 复制 WiFi 密码
            lv_wifi_connect(); // 显示wifi连接界面
            // 发送WiFi账号密码信息到队列
            xQueueSend(xQueueWifiAccount, &wifi_account, portMAX_DELAY); 
        }
    }
}

// 删除密码按钮处理函数
static void btn_del_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);// 获取事件代码

    if(code == LV_EVENT_CLICKED) { // 如果事件为点击
        ESP_LOGI(TAG, "Clicked");// 打印日志
        lv_textarea_del_char(ta_pass_text);// 删除密码输入框中的最后一个字符
    }
}

// 返回按钮处理函数
static void btn_back_cb(lv_event_t * e)
{
     lv_event_code_t code = lv_event_get_code(e);// 获取事件代码

    if(code == LV_EVENT_CLICKED) {// 如果事件为点击
        ESP_LOGI(TAG, "btn_back Clicked");// 打印日志
        lv_obj_del(wifi_password_page); // 删除密码输入界面
    }
}

// 进入输入密码界面
static void list_btn_cb(lv_event_t * e)
{
    // 获取点击到的WiFi名称
    const char *wifi_name=NULL;// 定义 WiFi 名称
    lv_event_code_t code = lv_event_get_code(e);// 获取事件代码
    lv_obj_t * obj = lv_event_get_target(e);// 获取事件目标对象
    if(code == LV_EVENT_CLICKED) {// 如果事件为点击
        wifi_name = lv_list_get_btn_text(wifi_list, obj);// 获取 WiFi 名称
        ESP_LOGI(TAG, "WLAN Name: %s", wifi_name);// 打印 WiFi 名称
    }

    // 创建密码输入页面
    wifi_password_page = lv_obj_create(lv_scr_act());// 创建密码输入页面
    lv_obj_set_size(wifi_password_page, 320, 240);// 设置页面大小
    lv_obj_set_style_border_width(wifi_password_page, 0, 0); // 设置边框宽度
    lv_obj_set_style_pad_all(wifi_password_page, 0, 0);  // 设置内间距
    lv_obj_set_style_radius(wifi_password_page, 0, 0); // 设置圆角半径

    // 创建返回按钮
    lv_obj_t *btn_back = lv_btn_create(wifi_password_page);// 创建返回按钮
    lv_obj_align(btn_back, LV_ALIGN_TOP_LEFT, 0, 0);// 对齐按钮
    lv_obj_set_size(btn_back, 60, 40);// 设置按钮大小
    lv_obj_set_style_border_width(btn_back, 0, 0); // 设置边框宽度
    lv_obj_set_style_pad_all(btn_back, 0, 0);   // 设置内间距
    lv_obj_set_style_bg_opa(btn_back, LV_OPA_TRANSP, LV_PART_MAIN);// 设置背景透明
    lv_obj_set_style_shadow_opa(btn_back, LV_OPA_TRANSP, LV_PART_MAIN); // 设置阴影透明
    lv_obj_add_event_cb(btn_back, btn_back_cb, LV_EVENT_ALL, NULL); // 添加按键处理函数

    lv_obj_t *label_back = lv_label_create(btn_back); // 创建返回按钮标签
    lv_label_set_text(label_back, LV_SYMBOL_LEFT);  // 设置标签文本为左箭头符号
    lv_obj_set_style_text_font(label_back, &lv_font_montserrat_20, 0);// 设置字体
    lv_obj_set_style_text_color(label_back, lv_color_hex(0x000000), 0); // 设置文本颜色
    lv_obj_align(label_back, LV_ALIGN_TOP_LEFT, 10, 10);// 对齐标签

    // 显示选中的wifi名称
    label_wifi_name = lv_label_create(wifi_password_page); // 创建 WiFi 名称标签
    lv_obj_set_style_text_font(label_wifi_name, &font_alipuhui20, 0); // 设置字体
    lv_label_set_text(label_wifi_name, wifi_name);// 设置标签文本为 WiFi 名称
    lv_obj_align(label_wifi_name, LV_ALIGN_TOP_MID, 0, 10);// 对齐标签

    // 创建密码输入框
    ta_pass_text = lv_textarea_create(wifi_password_page);// 创建密码输入框
    lv_obj_set_style_text_font(ta_pass_text, &lv_font_montserrat_20, 0);// 设置字体
    lv_textarea_set_one_line(ta_pass_text, true); // 设置为单行显示
    lv_textarea_set_password_mode(ta_pass_text, false); // 设置为非密码模式
    lv_textarea_set_placeholder_text(ta_pass_text, "password"); // 设置提示文本
    lv_obj_set_width(ta_pass_text, 150); // 设置宽度
    lv_obj_align(ta_pass_text, LV_ALIGN_TOP_LEFT, 10, 40); // 设置位置
    lv_obj_add_state(ta_pass_text, LV_STATE_FOCUSED); // 显示光标

    // 创建“连接按钮”
    lv_obj_t *btn_connect = lv_btn_create(wifi_password_page);// 创建连接按钮
    lv_obj_align(btn_connect, LV_ALIGN_TOP_LEFT, 170, 40); // 对齐按钮
    lv_obj_set_width(btn_connect, 65); // 设置宽度
    lv_obj_add_event_cb(btn_connect, btn_connect_cb, LV_EVENT_ALL, NULL); // 添加按键处理函数

    lv_obj_t *label_ok = lv_label_create(btn_connect);// 创建连接按钮标签
    lv_label_set_text(label_ok, "OK");// 设置按钮文本为 "OK"
    lv_obj_set_style_text_font(label_ok, &lv_font_montserrat_20, 0);// 设置字体
    lv_obj_center(label_ok);// 居中对齐标签

    // 创建“删除按钮”
    lv_obj_t *btn_del = lv_btn_create(wifi_password_page);// 创建删除按钮
    lv_obj_align(btn_del, LV_ALIGN_TOP_LEFT, 245, 40);// 对齐按钮
    lv_obj_set_width(btn_del, 65); // 设置宽度
    lv_obj_add_event_cb(btn_del, btn_del_cb, LV_EVENT_ALL, NULL); // 添加事件处理函数

    lv_obj_t *label_del = lv_label_create(btn_del);// 创建删除按钮标签
    lv_label_set_text(label_del, LV_SYMBOL_BACKSPACE);// 设置标签文本为后退符号
    lv_obj_set_style_text_font(label_del, &lv_font_montserrat_20, 0);// 设置字体
    lv_obj_center(label_del);// 居中对齐标签

    //  创建数字滚轮样式
    static lv_style_t style;// 定义样式变量
    lv_style_init(&style);// 初始化样式
    lv_style_set_bg_color(&style, lv_color_black());// 设置背景颜色为黑色
    lv_style_set_text_color(&style, lv_color_white()); // 设置文本颜色为白色
    lv_style_set_border_width(&style, 0);// 设置边框宽度为 0
    lv_style_set_pad_all(&style, 0);// 设置内间距为 0
    lv_style_set_radius(&style, 0);// 设置圆角半径为 0

    // 创建"数字"roller
    const char * opts_num = "0\n1\n2\n3\n4\n5\n6\n7\n8\n9";// 数字选项

    roller_num = lv_roller_create(wifi_password_page);// 创建数字滚轮
    lv_obj_add_style(roller_num, &style, 0); // 添加样式
    lv_obj_set_style_bg_opa(roller_num, LV_OPA_50, LV_PART_SELECTED);// 设置选中项的透明度

    lv_roller_set_options(roller_num, opts_num, LV_ROLLER_MODE_INFINITE);// 循环滚动模式
    lv_roller_set_visible_row_count(roller_num, 3);  // 设置可见行数为 3
    lv_roller_set_selected(roller_num, 5, LV_ANIM_OFF); // 默认选择第 5 项
    lv_obj_set_width(roller_num, 90);// 设置宽度
    lv_obj_set_style_text_font(roller_num, &lv_font_montserrat_20, 0);// 设置字体
    lv_obj_align(roller_num, LV_ALIGN_BOTTOM_LEFT, 15, -53);// 对齐滚轮
    lv_obj_add_event_cb(roller_num, mask_event_cb, LV_EVENT_ALL, NULL); // 添加事件处理函数

    //  创建数字滚轮的确认键
    lv_obj_t *btn_num_ok = lv_btn_create(wifi_password_page);// 创建确认按钮
    lv_obj_align(btn_num_ok, LV_ALIGN_BOTTOM_LEFT, 15, -10); // 对齐按钮
    lv_obj_set_width(btn_num_ok, 90); // 设置宽度
    lv_obj_add_event_cb(btn_num_ok, btn_num_cb, LV_EVENT_ALL, NULL); // 添加事件处理函数

    lv_obj_t *label_num_ok = lv_label_create(btn_num_ok);// 创建按钮标签
    lv_label_set_text(label_num_ok, LV_SYMBOL_OK); // 设置标签文本为 "OK"
    lv_obj_set_style_text_font(label_num_ok, &lv_font_montserrat_20, 0);// 设置字体
    lv_obj_center(label_num_ok); // 居中对齐标签

    // 创建小写字母滚轮
    const char * opts_letter_low = "a\nb\nc\nd\ne\nf\ng\nh\ni\nj\nk\nl\nm\nn\no\np\nq\nr\ns\nt\nu\nv\nw\nx\ny\nz"; // 小写字母选项

    roller_letter_low = lv_roller_create(wifi_password_page);// 创建小写字母滚轮
    lv_obj_add_style(roller_letter_low, &style, 0);// 添加样式
    lv_obj_set_style_bg_opa(roller_letter_low, LV_OPA_50, LV_PART_SELECTED); // 设置选中项的透明度
    lv_roller_set_options(roller_letter_low, opts_letter_low, LV_ROLLER_MODE_INFINITE); // 循环滚动模式
    lv_roller_set_visible_row_count(roller_letter_low, 3);// 设置可见行数为 3
    lv_roller_set_selected(roller_letter_low, 15, LV_ANIM_OFF); // 默认选择第 15 项
    lv_obj_set_width(roller_letter_low, 90);// 设置宽度
    lv_obj_set_style_text_font(roller_letter_low, &lv_font_montserrat_20, 0);// 设置字体
    lv_obj_align(roller_letter_low, LV_ALIGN_BOTTOM_LEFT, 115, -53);// 对齐滚轮
    lv_obj_add_event_cb(roller_letter_low, mask_event_cb, LV_EVENT_ALL, NULL); // 事件处理函数

    //  创建小写字母滚轮的确认键
    lv_obj_t *btn_letter_low_ok = lv_btn_create(wifi_password_page);// 创建确认按钮
    lv_obj_align(btn_letter_low_ok, LV_ALIGN_BOTTOM_LEFT, 115, -10);// 对齐按钮
    lv_obj_set_width(btn_letter_low_ok, 90); // 设置宽度
    lv_obj_add_event_cb(btn_letter_low_ok, btn_letter_low_cb, LV_EVENT_ALL, NULL); // 添加事件处理函数

    lv_obj_t *label_letter_low_ok = lv_label_create(btn_letter_low_ok); // 创建按钮标签
    lv_label_set_text(label_letter_low_ok, LV_SYMBOL_OK); // 设置标签文本为 "OK"
    lv_obj_set_style_text_font(label_letter_low_ok, &lv_font_montserrat_20, 0);// 设置字体
    lv_obj_center(label_letter_low_ok);// 居中对齐标签

    // 创建大写字母滚轮
    const char * opts_letter_up = "A\nB\nC\nD\nE\nF\nG\nH\nI\nJ\nK\nL\nM\nN\nO\nP\nQ\nR\nS\nT\nU\nV\nW\nX\nY\nZ";// 大写字母选项

    roller_letter_up = lv_roller_create(wifi_password_page);// 创建大写字母滚轮
    lv_obj_add_style(roller_letter_up, &style, 0);// 添加样式
    lv_obj_set_style_bg_opa(roller_letter_up, LV_OPA_50, LV_PART_SELECTED); // 设置选中项的透明度
    lv_roller_set_options(roller_letter_up, opts_letter_up, LV_ROLLER_MODE_INFINITE); // 循环滚动模式
    lv_roller_set_visible_row_count(roller_letter_up, 3);// 设置可见行数为 3
    lv_roller_set_selected(roller_letter_up, 15, LV_ANIM_OFF); // 默认选择第 15 项
    lv_obj_set_width(roller_letter_up, 90); // 设置宽度
    lv_obj_set_style_text_font(roller_letter_up, &lv_font_montserrat_20, 0);// 设置字体
    lv_obj_align(roller_letter_up, LV_ALIGN_BOTTOM_LEFT, 215, -53); // 对齐滚轮
    lv_obj_add_event_cb(roller_letter_up, mask_event_cb, LV_EVENT_ALL, NULL); // 事件处理函数

    // 创建大写字母滚轮的确认键
    lv_obj_t *btn_letter_up_ok = lv_btn_create(wifi_password_page);// 创建确认按钮
    lv_obj_align(btn_letter_up_ok, LV_ALIGN_BOTTOM_LEFT, 215, -10);// 对齐按钮
    lv_obj_set_width(btn_letter_up_ok, 90); // 设置宽度
    lv_obj_add_event_cb(btn_letter_up_ok, btn_letter_up_cb, LV_EVENT_ALL, NULL); // 事件处理函数

    lv_obj_t *label_letter_up_ok = lv_label_create(btn_letter_up_ok);// 创建按钮标签
    lv_label_set_text(label_letter_up_ok, LV_SYMBOL_OK);// 设置标签文本为 "OK"
    lv_obj_set_style_text_font(label_letter_up_ok, &lv_font_montserrat_20, 0);// 设置字体
    lv_obj_center(label_letter_up_ok);// 居中对齐标签

}

// 事件处理函数
static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    static int s_retry_num = 0;// 定义重试次数

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {//收到连接开始⌚️
        xEventGroupSetBits(s_wifi_event_group, WIFI_START_BIT);// 设置 WiFi 开始连接标志
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {//收到连接断开事件
        if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY) {// 如果重试次数小于最大重试次数
            esp_wifi_connect();// 重新连接 WiFi
            s_retry_num++;// 增加重试次数
            ESP_LOGI(TAG, "retry to connect to the AP");// 打印重试连接信息
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);// 设置 WiFi 连接失败标志
        }
        ESP_LOGI(TAG,"connect to the AP fail");// 打印连接失败信息
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) { //收到获取ip地址成功时间
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;// 获取 IP 事件数据
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));// 打印获取到的 IP 地址
        s_retry_num = 0;// 重置重试次数
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);// 设置 WiFi 连接成功标志
    }
}

// 扫描附近wifi
static void wifi_scan(wifi_ap_record_t ap_info[], uint16_t *ap_number)
{
    s_wifi_event_group = xEventGroupCreate();// 创建 WiFi 事件组

    ESP_ERROR_CHECK(esp_netif_init());// 初始化网络接口
    ESP_ERROR_CHECK(esp_event_loop_create_default());// 创建默认事件循环
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();// 创建默认的 WiFi STA 网络接口
    assert(sta_netif);// 确保网络接口创建成功

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();// 初始化 WiFi 配置
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));// 初始化 WiFi

    esp_event_handler_instance_t instance_any_id;// 定义事件处理实例
    esp_event_handler_instance_t instance_got_ip;// 定义获取 IP 事件处理实例
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,// 注册 WiFi 事件处理
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,// 注册 IP 事件处理
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));
    
    uint16_t ap_count = 0;// 定义扫描到的 AP 数量
    
    memset(ap_info, 0, *ap_number * sizeof(wifi_ap_record_t));// 清空 AP 信息

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA)); // 设置 WiFi 模式为 STA
    ESP_ERROR_CHECK(esp_wifi_start());// 启动 WiFi
    esp_wifi_scan_start(NULL, true);// 开始扫描 WiFi

    ESP_LOGI(TAG, "Max AP number ap_info can hold = %u", *ap_number);// 打印最大 AP 数量
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));  // 获取扫描到的wifi数量
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(ap_number, ap_info)); // 获取真实的获取到wifi数量和信息
    ESP_LOGI(TAG, "Total APs scanned = %u, actual AP number ap_info holds = %u", ap_count, *ap_number);// 打印扫描结果
}

// lcd处理任务
static void wifi_connect(void *arg)
{
    wifi_account_t wifi_account;// 创建 WiFi 账号结构体

    while (true)// 无限循环
    {
        // 如果收到wifi账号队列消息
        if(xQueueReceive(xQueueWifiAccount, &wifi_account, portMAX_DELAY))
        {
            wifi_config_t wifi_config = {// 创建 WiFi 配置结构体
                .sta = {
                    .threshold.authmode = WIFI_AUTH_WPA2_PSK,// 设置认证模式
                    .sae_pwe_h2e = WPA3_SAE_PWE_BOTH,// 设置 WPA3 PWE
                    .sae_h2e_identifier = "",// SAE H2E 标识符
                    },
            };
            strcpy((char *)wifi_config.sta.ssid, wifi_account.wifi_ssid);// 复制 WiFi 名称
            strcpy((char *)wifi_config.sta.password, wifi_account.wifi_password);// 复制 WiFi 密码
            ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) ); // 设置 WiFi 配置
            ESP_LOGI(TAG, "connected to ap SSID:%s password:%s",
                 wifi_config.sta.ssid, wifi_config.sta.password);// 打印连接信息
            esp_wifi_connect(); // 开始连接 WiFi
            /* 等待连接成功（WIFI_CONNECTED_BIT）或连接失败（WIFI_FAIL_BIT）*/
            EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

            /* xEventGroupWaitBits() 返回的是调用之前的位，因此我们可以测试实际发生的事件。 */
            if (bits & WIFI_CONNECTED_BIT) {// 如果连接成功
                ESP_LOGI(TAG, "connected to ap SSID:%s password:%s",
                        wifi_config.sta.ssid, wifi_config.sta.password); // 打印连接成功信息
                lvgl_port_lock(0);// 锁定 LVGL
                lv_label_set_text(label_wifi_connect, "WLAN连接成功");// 更新连接成功标签
                lvgl_port_unlock();// 解锁 LVGL
            } else if (bits & WIFI_FAIL_BIT) {// 如果连接失败
                ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
                        wifi_config.sta.ssid, wifi_config.sta.password);// 打印连接失败信息
                lvgl_port_lock(0);// 锁定 LVGL
                lv_label_set_text(label_wifi_connect, "WLAN连接失败"); // 更新连接失败标签
                lvgl_port_unlock();// 解锁 LVGL
            } else {// 其他情况
                ESP_LOGE(TAG, "UNEXPECTED EVENT");// 打印意外事件信息
                lvgl_port_lock(0);// 锁定 LVGL
                lv_label_set_text(label_wifi_connect, "WLAN连接异常");// 更新连接异常标签
                lvgl_port_unlock();// 解锁 LVGL
            }
        }
    }
}

// wifi连接
void app_wifi_connect(void)
{
    lvgl_port_lock(0); // 锁定 LVGL
    // 创建WLAN扫描页面
    static lv_style_t style;// 定义样式变量
    lv_style_init(&style);// 初始化样式
    lv_style_set_bg_opa( &style, LV_OPA_COVER ); // 设置背景透明度
    lv_style_set_border_width(&style, 0); // 设置边框宽度
    lv_style_set_pad_all(&style, 0);  // 设置内间距
    lv_style_set_radius(&style, 0);   // 设置圆角半径
    lv_style_set_width(&style, 320);  // 设置宽度
    lv_style_set_height(&style, 240); // 设置高度
    wifi_scan_page = lv_obj_create(lv_scr_act());  // 创建 WiFi 扫描页面
    lv_obj_add_style(wifi_scan_page, &style, 0);// 添加样式
    // 在WLAN扫描页面显示提示
    lv_obj_t *label_wifi_scan = lv_label_create(wifi_scan_page);// 创建标签
    lv_label_set_text(label_wifi_scan, "WLAN扫描中...");// 设置标签文本
    lv_obj_set_style_text_font(label_wifi_scan, &font_alipuhui20, 0);// 设置字体
    lv_obj_align(label_wifi_scan, LV_ALIGN_CENTER, 0, -50);// 对齐标签
    lvgl_port_unlock();// 解锁 LVGL

    // 扫描WLAN信息
    wifi_ap_record_t ap_info[DEFAULT_SCAN_LIST_SIZE];  // 记录扫描到的wifi信息
    uint16_t ap_number = DEFAULT_SCAN_LIST_SIZE; // 最大扫描数量
    wifi_scan(ap_info, &ap_number); // 扫描附近wifi

    lvgl_port_lock(0);// 锁定 LVGL
    // 扫描附近wifi信息成功后 删除提示文字
    lv_obj_del(label_wifi_scan);  // 删除提示标签
    // 创建wifi信息列表
    wifi_list = lv_list_create(wifi_scan_page);// 创建列表
    lv_obj_set_size(wifi_list, lv_pct(100), lv_pct(100));// 设置列表大小
    lv_obj_set_style_border_width(wifi_list, 0, 0);// 设置边框宽度
    lv_obj_set_style_text_font(wifi_list, &font_alipuhui20, 0);// 设置字体
    lv_obj_set_scrollbar_mode(wifi_list, LV_SCROLLBAR_MODE_OFF); // 隐藏 WiFi 列表滚动条
    // 显示wifi信息
    lv_obj_t * btn;// 定义按钮
    for (int i = 0; i < ap_number; i++) {// 遍历扫描到的 WiFi 信息
        ESP_LOGI(TAG, "SSID \t\t%s", ap_info[i].ssid);  // 终端输出wifi名称
        ESP_LOGI(TAG, "RSSI \t\t%d", ap_info[i].rssi);  // 终端输出wifi信号质量
        // 添加wifi列表
        btn = lv_list_add_btn(wifi_list, LV_SYMBOL_WIFI, (const char *)ap_info[i].ssid);  // 添加按钮
        lv_obj_add_event_cb(btn, list_btn_cb, LV_EVENT_CLICKED, NULL); // 添加点击回调函数
    }
    lvgl_port_unlock();// 解锁 LVGL
    
    // 创建wifi连接任务
    xQueueWifiAccount = xQueueCreate(2, sizeof(wifi_account_t));// 创建 WiFi 账号队列
    xTaskCreatePinnedToCore(wifi_connect, "wifi_connect", 4 * 1024, NULL, 5, NULL, 1);  // 创建wifi连接任务
}




