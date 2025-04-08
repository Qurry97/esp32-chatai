#include "lcd_gc9107_display.h"

#include <vector>
#include <font_awesome_symbols.h>
#include <esp_log.h>
#include <esp_err.h>
#include <esp_lvgl_port.h>
#include "assets/lang_config.h"

#include "board.h"
#include "resources.h"
#include "audio_codec.h"

#define TAG "LcdGc9107Display"

LV_FONT_DECLARE(font_awesome_30_4);


SpiLcdGc9107Display::SpiLcdGc9107Display(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                           int width, int height, int offset_x, int offset_y, bool mirror_x, bool mirror_y, bool swap_xy,
                           DisplayFonts fonts)
    : LcdGc9107Display(panel_io, panel, fonts) {
    width_ = width;
    height_ = height;

    // draw white
    std::vector<uint16_t> buffer(width_, 0xFFFF);
    for (int y = 0; y < height_; y++) {
        esp_lcd_panel_draw_bitmap(panel_, 0, y, width_, y + 1, buffer.data());
    }

    // Set the display to on
    ESP_LOGI(TAG, "Turning display on");
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_, true));

    ESP_LOGI(TAG, "Initialize LVGL library");
    lv_init();

    ESP_LOGI(TAG, "Initialize LVGL port");
    lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    port_cfg.task_priority = 1;
    lvgl_port_init(&port_cfg);

    ESP_LOGI(TAG, "Adding LCD screen");
    const lvgl_port_display_cfg_t display_cfg = {
        .io_handle = panel_io_,
        .panel_handle = panel_,
        .control_handle = nullptr,
        .buffer_size = static_cast<uint32_t>(width_ * 10),
        .double_buffer = false,
        .trans_size = 0,
        .hres = static_cast<uint32_t>(width_),
        .vres = static_cast<uint32_t>(height_),
        .monochrome = false,
        .rotation = {
            .swap_xy = swap_xy,
            .mirror_x = mirror_x,
            .mirror_y = mirror_y,
        },
        .color_format = LV_COLOR_FORMAT_RGB565,
        .flags = {
            .buff_dma = 1,
            .buff_spiram = 0,
            .sw_rotate = 0,
            .swap_bytes = 1,
            .full_refresh = 0,
            .direct_mode = 0,
        },
    };

    display_ = lvgl_port_add_disp(&display_cfg);
    if (display_ == nullptr) {
        ESP_LOGE(TAG, "Failed to add display");
        return;
    }

    if (offset_x != 0 || offset_y != 0) {
        lv_display_set_offset(display_, offset_x, offset_y);
    }
    ShowLogo();
    esp_timer_create_args_t logo_timer_args = {
        .callback = [](void *arg) {
            LcdGc9107Display *display = static_cast<LcdGc9107Display*>(arg);
            DisplayLockGuard lock(display);
            if(display->current_logo_index_< 21){
                display->current_logo_index_++;
                display->SetLogoImg(display->current_logo_index_);
            }else{
                if (display->logo_img_ != nullptr) {
                    lv_obj_del(display->logo_img_);
                    display->logo_img_ = nullptr;
                }
                esp_timer_stop(display->logo_timer_);
                esp_timer_delete(display->logo_timer_);
                display->logo_show_status = true;
                display->SetupUI();
            }    
        },
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "logo_timer",
        .skip_unhandled_events = false,
    };
    ESP_ERROR_CHECK(esp_timer_create(&logo_timer_args, &logo_timer_));
    ESP_ERROR_CHECK(esp_timer_start_periodic(logo_timer_, LOGO_TIMEOUT));
}


LcdGc9107Display::~LcdGc9107Display() {
    // 然后再清理 LVGL 对象
    if (content_ != nullptr) {
        lv_obj_del(content_);
    }
    if (status_bar_ != nullptr) {
        lv_obj_del(status_bar_);
    }
    if (side_bar_ != nullptr) {
        lv_obj_del(side_bar_);
    }
    if (container_ != nullptr) {
        lv_obj_del(container_);
    }
    if (display_ != nullptr) {
        lv_display_delete(display_);
    }

    if (panel_ != nullptr) {
        esp_lcd_panel_del(panel_);
    }
    if (panel_io_ != nullptr) {
        esp_lcd_panel_io_del(panel_io_);
    }
    if (face_timer_ != nullptr) {
        esp_timer_stop(face_timer_);
        esp_timer_delete(face_timer_);
    }
}

bool LcdGc9107Display::Lock(int timeout_ms) {
    return lvgl_port_lock(timeout_ms);
}

void LcdGc9107Display::Unlock() {
    lvgl_port_unlock();
}

void LcdGc9107Display::SetLogoImg(int index) 
{
    DisplayLockGuard lock(this);
    if(logo_img_==nullptr){
        return;
    }
    static const lv_img_dsc_t *images[] = {
        &bootlogo1, &bootlogo2, &bootlogo3, &bootlogo4, &bootlogo5,
        &bootlogo6, &bootlogo7, &bootlogo8, &bootlogo9, &bootlogo10,
        &bootlogo11, &bootlogo12, &bootlogo13, &bootlogo14, &bootlogo15,
        &bootlogo16, &bootlogo17, &bootlogo18, &bootlogo19, &bootlogo20,
        &bootlogo21, &bootlogo22
    };
    lv_img_set_src(logo_img_, images[index]);
}

void LcdGc9107Display::ShowLogo() 
{
    DisplayLockGuard lock(this);

    auto screen = lv_screen_active();
    lv_obj_set_scrollbar_mode(screen, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_color(screen,lv_color_black(), 0);
    lv_obj_set_style_bg_opa(screen, 255, 0);

    logo_img_ = lv_img_create(screen);
    lv_img_set_src(logo_img_, &bootlogo1);
    lv_obj_align(logo_img_, LV_ALIGN_CENTER, 0, 0);

}

void LcdGc9107Display::SetupUI() {
    DisplayLockGuard lock(this);

    auto screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen,lv_color_black() ,0);
    lv_obj_set_style_bg_opa(screen, 255, 0);
    lv_obj_set_style_text_font(screen, fonts_.text_font, 0);
    lv_obj_set_style_text_color(screen, lv_color_white(), 0);
    lv_obj_set_style_text_opa(screen, 200, 0);

    /* Container */
    container_ = lv_obj_create(screen);
    lv_obj_set_style_radius(container_, 0, 0);
    lv_obj_set_style_bg_color(container_,lv_color_black() ,0);
    lv_obj_set_style_bg_opa(container_, 255, 0);
    lv_obj_set_size(container_, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_border_width(container_, 0, 0);

    /* Status bar */
    status_bar_ = lv_obj_create(container_);
    lv_obj_set_style_bg_color(status_bar_,lv_color_black() ,0);
    lv_obj_set_style_bg_opa(status_bar_, 20, 0);
    lv_obj_set_size(status_bar_,  LV_PCT(15),LV_VER_RES);
    lv_obj_set_style_radius(status_bar_, 0, 0);
    lv_obj_set_style_border_width(status_bar_, 0, 0);
    lv_obj_set_align(status_bar_, LV_ALIGN_LEFT_MID);
    lv_obj_set_flex_flow(status_bar_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(status_bar_, 0, 0);
    lv_obj_set_style_pad_column(status_bar_, 0, 0);
    lv_obj_set_style_pad_top(status_bar_, 2, 0);
    lv_obj_set_style_pad_bottom(status_bar_, 2, 0);

    network_label_ = lv_label_create(status_bar_);
    lv_label_set_text(network_label_, "");
    lv_obj_set_style_text_font(network_label_, fonts_.icon_font, 0);

    mute_label_ = lv_label_create(status_bar_);
    lv_label_set_text(mute_label_, "");
    lv_obj_set_style_text_font(mute_label_, fonts_.icon_font, 0);

    battery_label_ = lv_label_create(status_bar_);
    lv_label_set_text(battery_label_, "");
    lv_obj_set_style_text_font(battery_label_, fonts_.icon_font, 0);

    notification_label_ = lv_label_create(status_bar_);
    lv_obj_set_flex_grow(notification_label_, 1);
    lv_obj_set_style_text_align(notification_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(notification_label_, "");
    lv_obj_add_flag(notification_label_, LV_OBJ_FLAG_HIDDEN);

    status_label_ = lv_label_create(status_bar_);
    lv_obj_set_flex_grow(status_label_, 1);
    lv_label_set_long_mode(status_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_style_text_align(status_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(status_label_, Lang::Strings::INITIALIZING);


    
    /* Content */
    content_ = lv_obj_create(screen);
    lv_obj_set_scrollbar_mode(content_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_radius(content_, 0, 0);
    lv_obj_set_style_border_width(content_, 0, 0);
    lv_obj_set_style_bg_opa(content_, 0, 0);
    lv_obj_set_size(content_, LV_HOR_RES, LV_VER_RES);

    face_img_ = lv_img_create(content_);
    lv_img_set_src(face_img_, &neutral1);
    lv_obj_set_width(face_img_, LV_SIZE_CONTENT);   /// 128
    lv_obj_set_height(face_img_, LV_SIZE_CONTENT);    /// 64
    lv_obj_set_align(face_img_, LV_ALIGN_CENTER);
    lv_obj_add_flag(face_img_, LV_OBJ_FLAG_ADV_HITTEST);     /// Flags
    
    low_battery_popup_ = lv_obj_create(screen);
    lv_obj_set_scrollbar_mode(low_battery_popup_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_size(low_battery_popup_, LV_HOR_RES * 0.9, fonts_.text_font->line_height);
    lv_obj_align(low_battery_popup_, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(low_battery_popup_, lv_color_black(), 0);
    lv_obj_set_style_radius(low_battery_popup_, 10, 0);
    lv_obj_t* low_battery_label = lv_label_create(low_battery_popup_);
    lv_label_set_text(low_battery_label, Lang::Strings::BATTERY_NEED_CHARGE);
    lv_obj_set_style_text_color(low_battery_label, lv_color_white(), 0);
    lv_obj_center(low_battery_label);
    lv_obj_add_flag(low_battery_popup_, LV_OBJ_FLAG_HIDDEN);

    vol_arc_ = lv_arc_create(lv_layer_top());
    lv_obj_set_size(vol_arc_, 48, 48);
    lv_obj_align(vol_arc_, LV_ALIGN_CENTER, 0, 0);
    lv_arc_set_rotation(vol_arc_, 270);
    lv_arc_set_range(vol_arc_, 0, 100);
    lv_arc_set_bg_angles(vol_arc_, 0, 360);
    lv_obj_remove_style(vol_arc_, NULL, LV_PART_KNOB);
    lv_obj_remove_flag(vol_arc_, LV_OBJ_FLAG_CLICKABLE);
    auto codec = Board::GetInstance().GetAudioCodec();
    lv_arc_set_value(vol_arc_, codec->GetOutputVolume());
    lv_obj_add_flag(vol_arc_, LV_OBJ_FLAG_HIDDEN);

    vol_label_ = lv_label_create(vol_arc_);
    lv_label_set_text_fmt(vol_label_,"%d",codec->GetOutputVolume());
    lv_obj_set_style_text_color(vol_label_, lv_color_white(), 0);
    lv_obj_center(vol_label_);

    esp_timer_create_args_t face_timer_args = {
        .callback = [](void *arg) {
            LcdGc9107Display *display = static_cast<LcdGc9107Display*>(arg);
            DisplayLockGuard lock(display);
            if(display->current_face_index_ <display->current_face_count_-1){
                display->current_face_index_++;
                display->ShowFace(display->current_face_state_);
            }else{
                display->current_face_index_ = 0;
                display->current_face_state_ = NEUTRAL;
                display->ShowFace(display->current_face_state_);
            }
        },
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "face_timer",
        .skip_unhandled_events = false,
    };
    ESP_ERROR_CHECK(esp_timer_create(&face_timer_args, &face_timer_));
    ESP_ERROR_CHECK(esp_timer_start_periodic(face_timer_, FACE_TIMEOUT));
}


void LcdGc9107Display::SetNeutral(int index)
{
    DisplayLockGuard lock(this);
    if(face_img_==nullptr){
        return;
    } 
    static const lv_img_dsc_t *images[] = {
        &neutral1, &neutral2, &neutral3, &neutral4, &neutral5,
        &neutral6, &neutral7, &neutral8, &neutral9, &neutral10,
        &neutral11, &neutral12, &neutral13, &neutral14
    };
    if(current_face_count_ != sizeof(images) / sizeof(images[0]))
        current_face_count_ = sizeof(images) / sizeof(images[0]);

    lv_img_set_src(face_img_, images[index]);
}

void LcdGc9107Display::SetHappy(int index)
{
    DisplayLockGuard lock(this);
    if(face_img_==nullptr){
        return;
    } 
    static const lv_img_dsc_t *images[] = {
        &happy1, &happy2, &happy3, &happy4, &happy5,
        &happy6, &happy7, &happy8, &happy9, &happy10,
        &happy11, &happy12, &happy13, &happy14, &happy15,
        &happy16
    };
    if(current_face_count_ != sizeof(images) / sizeof(images[0]))
        current_face_count_ = sizeof(images) / sizeof(images[0]);

    lv_img_set_src(face_img_, images[index]);
}

void LcdGc9107Display::SetSad(int index)
{
    DisplayLockGuard lock(this);
    if(face_img_==nullptr){
        return;
    } 
    static const lv_img_dsc_t *images[] = {
        &sad1, &sad2, &sad3, &sad4, &sad5,
        &sad6, &sad7, &sad8, &sad9, &sad10,
        &sad11, &sad12, &sad13, &sad14, &sad15,
        &sad16
    };
    if(current_face_count_ != sizeof(images) / sizeof(images[0]))
        current_face_count_ = sizeof(images) / sizeof(images[0]);

    lv_img_set_src(face_img_, images[index]);
}

void LcdGc9107Display::SetAngry(int index)
{
    DisplayLockGuard lock(this);
    if(face_img_==nullptr){
        return;
    } 
    static const lv_img_dsc_t *images[] = {
        &angry1, &angry2, &angry3, &angry4, &angry5,
        &angry6, &angry7, &angry8, &angry7, &angry6,
        &angry5, &angry4, &angry3, &angry2, &angry1,
    };
    if(current_face_count_ != sizeof(images) / sizeof(images[0]))
        current_face_count_ = sizeof(images) / sizeof(images[0]);

    lv_img_set_src(face_img_, images[index]);
}

void LcdGc9107Display::SetLoving(int index)
{
    DisplayLockGuard lock(this);
    if(face_img_==nullptr){
        return;
    } 
    static const lv_img_dsc_t *images[] = {
        &loving1, &loving2, &loving3, &loving4, &loving5,
        &loving6, &loving7, &loving8, &loving9, &loving7, 
        &loving6,&loving5, &loving4, &loving3, &loving2, 
        &loving1,
    };
    if(current_face_count_ != sizeof(images) / sizeof(images[0]))
        current_face_count_ = sizeof(images) / sizeof(images[0]);

    lv_img_set_src(face_img_, images[index]);
}

void LcdGc9107Display::SetEmbarrass(int index)
{
    DisplayLockGuard lock(this);
    if(face_img_==nullptr){
        return;
    } 
    static const lv_img_dsc_t *images[] = {
        &embarrassed1, &embarrassed2, &embarrassed3, &embarrassed4, &embarrassed5,
        &embarrassed6, &embarrassed5, &embarrassed4, &embarrassed4, &embarrassed5, 
        &embarrassed4, &embarrassed3, &embarrassed2, &embarrassed1,
    };
    if(current_face_count_ != sizeof(images) / sizeof(images[0]))
        current_face_count_ = sizeof(images) / sizeof(images[0]);

    lv_img_set_src(face_img_, images[index]);
}

void LcdGc9107Display::SetDefault(int index)
{
    DisplayLockGuard lock(this);
    if(face_img_==nullptr){
        return;
    } 
    static const lv_img_dsc_t *images[] = {
        &default_face1, &default_face2, &default_face3, &default_face4, &default_face5,
        &default_face6, &default_face7, &default_face8, &default_face7, &default_face5, 
        &default_face4, &default_face3, &default_face2, &default_face1,
    };
    if(current_face_count_ != sizeof(images) / sizeof(images[0]))
        current_face_count_ = sizeof(images) / sizeof(images[0]);

    lv_img_set_src(face_img_, images[index]);
}

void LcdGc9107Display::ShowFace(int index)
{
    DisplayLockGuard lock(this);
    if(face_img_==nullptr){
        return;
    } 
    switch(index)
    {
        case NEUTRAL:
            SetNeutral(current_face_index_);
            break;
        case HAPPY:
        case LAUGHING:
        case FUNNY:
            SetHappy(current_face_index_);
            break;
        case SAD:
            SetSad(current_face_index_);
            break;
        case ANGRY:
            SetAngry(current_face_index_);
            break;
        case CRYING:
            
            break;
        case EMBARRAS:
            SetEmbarrass(current_face_index_);
            break;
        case LOVING:
            SetLoving(current_face_index_);
            break;
        default:
            SetDefault(current_face_index_);
            break;
    }
}

void LcdGc9107Display::SetFace(const char* emoji) 
{
    DisplayLockGuard lock(this);
    static const std::vector<std::string>  emojis = {
        {"neutral"},
        {"happy"},
        {"laughing"},
        {"funny"},
        {"sad"},
        {"angry"},
        {"crying"},
        {"loving"},
        {"embarrassed"},
        {"surprised"},
        {"shocked"},
        {"thinking"},
        {"winking"},
        {"cool"},
        {"relaxed"},
        {"delicious"},
        {"kissy"},
        {"confident"},
        {"sleepy"},
        {"silly"},
        {"confused"}
    };
    
    std::string emojiStr(emoji);

    // 使用std::find查找emojiStr在emojis中的位置
    auto it = std::find(emojis.begin(), emojis.end(), emojiStr);

    // 如果找到了，返回索引
    if (it != emojis.end()) {
        current_face_state_ =  std::distance(emojis.begin(), it);
        current_face_index_ = 0;
        ESP_LOGI(TAG, "emoji index:%d,input str:%s",current_face_state_,emoji);
        return;
    }
    current_face_state_ = 0;
}

bool LcdGc9107Display::GetLogoStatus() 
{
    DisplayLockGuard lock(this);
    return logo_show_status;
}

#if 0
void LcdGc9107Display::SetEmotion(const char* emotion) {
    struct Emotion {
        const char* icon;
        const char* text;
    };

    static const std::vector<Emotion> emotions = {
        {"😶", "neutral"},
        {"🙂", "happy"},
        {"😆", "laughing"},
        {"😂", "funny"},
        {"😔", "sad"},
        {"😠", "angry"},
        {"😭", "crying"},
        {"😍", "loving"},
        {"😳", "embarrassed"},
        {"😯", "surprised"},
        {"😱", "shocked"},
        {"🤔", "thinking"},
        {"😉", "winking"},
        {"😎", "cool"},
        {"😌", "relaxed"},
        {"🤤", "delicious"},
        {"😘", "kissy"},
        {"😏", "confident"},
        {"😴", "sleepy"},
        {"😜", "silly"},
        {"🙄", "confused"}
    };
    
    // 查找匹配的表情
    std::string_view emotion_view(emotion);
    auto it = std::find_if(emotions.begin(), emotions.end(),
        [&emotion_view](const Emotion& e) { return e.text == emotion_view; });

    DisplayLockGuard lock(this);
    if (emotion_label_ == nullptr) {
        return;
    }

    // 如果找到匹配的表情就显示对应图标，否则显示默认的neutral表情
    lv_obj_set_style_text_font(emotion_label_, fonts_.emoji_font, 0);
    if (it != emotions.end()) {
        lv_label_set_text(emotion_label_, it->icon);
    } else {
        lv_label_set_text(emotion_label_, "😶");
    }
}

void LcdGc9107Display::SetIcon(const char* icon) {
    DisplayLockGuard lock(this);
    if (emotion_label_ == nullptr) {
        return;
    }
    lv_obj_set_style_text_font(emotion_label_, &font_awesome_30_4, 0);
    lv_label_set_text(emotion_label_, icon);
}
#endif
