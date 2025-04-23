#ifndef LCD_GC9107_DISPLAY_H
#define LCD_GC9107_DISPLAY_H

#include "display.h"

#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <font_emoji.h>
#include <lvgl.h>
#include <atomic>

#define LOGO_TIMEOUT 130*1000    //us
#define FACE_TIMEOUT 220*1000    //us
enum {
    NEUTRAL =0,
    HAPPY,
    LAUGHING,
    FUNNY,
    SAD,
    ANGRY,
    CRYING,
    LOVING,
    EMBARRAS,
    SURPRISE,
    SHOCKED,
    THINKING,
    WINKING,
    COOL,
    RELAXED,
    DELICIOUS,
    KISSY,
    CONFIDENT,
    SLEEPY,
    SILLY,
    CONFUSED
};

class LcdGc9107Display : public Display {
protected:
    esp_lcd_panel_io_handle_t panel_io_ = nullptr;
    esp_lcd_panel_handle_t panel_ = nullptr;
    
    lv_draw_buf_t draw_buf_;
    // lv_obj_t* status_bar_ = nullptr;
    lv_obj_t* content_ = nullptr;
    lv_obj_t* container_ = nullptr;
    lv_obj_t* side_bar_ = nullptr;
    
    DisplayFonts fonts_;
    
    virtual void ShowLogo();
    virtual void ShowFace(int index);
    virtual void SetNeutral(int index);
    virtual void SetHappy(int index);
    virtual void SetSad(int index);
    virtual void SetAngry(int index);
    virtual void SetLoving(int index);
    virtual void SetEmbarrass(int index);
    virtual void SetDefault(int index);
    virtual bool Lock(int timeout_ms = 0) override;
    virtual void Unlock() override;

protected:
    // 添加protected构造函数
    LcdGc9107Display(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel, DisplayFonts fonts)
        : panel_io_(panel_io), panel_(panel), fonts_(fonts) {}

    
public:
    ~LcdGc9107Display();
    bool logo_show_status = false;
    lv_obj_t* logo_img_ = nullptr;
    
    int current_logo_index_ = 0;
    int current_face_index_ =0;
    int current_face_count_ =0;
    int current_face_state_ =0;

    esp_timer_handle_t logo_timer_ = nullptr;
    esp_timer_handle_t face_timer_ = nullptr;


    virtual void SetupUI();
    virtual void SetLogoImg(int index);
    void SetFace(const char* emoji) override;
    bool GetLogoStatus() override;
    // virtual void SetEmotion(const char* emotion) override;
    // virtual void SetIcon(const char* icon) override;
};



// // SPI LCD显示器
class SpiLcdGc9107Display : public LcdGc9107Display {
public:
    SpiLcdGc9107Display(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                  int width, int height, int offset_x, int offset_y,
                  bool mirror_x, bool mirror_y, bool swap_xy,
                  DisplayFonts fonts);
};

#endif // LCD_DISPLAY_H
