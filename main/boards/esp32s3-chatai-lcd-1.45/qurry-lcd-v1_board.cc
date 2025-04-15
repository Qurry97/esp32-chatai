#include "wifi_board.h"
#include "ml307_board.h"

#include "audio_codecs/no_audio_codec.h"

#include "system_reset.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "iot/thing_manager.h"
#include "led/single_led.h"

#include <esp_log.h>
#include <esp_lcd_panel_vendor.h>
#include <driver/i2c_master.h>
#include <wifi_station.h>

#ifdef LCD_GC9107
#include "esp_lcd_gc9107.h"
#include "display/lcd_gc9107_display.h"
#else
#include "display/lcd_display.h"
#endif
#ifdef POWER_CONFIG
#include "power_manager.h"
#include "power_save_timer.h"
#endif
#include <esp_sleep.h>

#define TAG "kevin-sp-v3"

LV_FONT_DECLARE(font_puhui_20_4);
LV_FONT_DECLARE(font_awesome_20_4);
LV_FONT_DECLARE(font_Bouti_14_1);
LV_FONT_DECLARE(font_awesome_14_1);

// class KEVIN_SP_V3Board : public Ml307Board {
class KEVIN_SP_V3Board : public WifiBoard {
private:
    i2c_master_bus_handle_t display_i2c_bus_;
    Button boot_button_;
    Button vol_up_button_;
    Button vol_dn_button_;
    #ifdef POWER_CONFIG
    PowerManager* power_manager_;
    PowerSaveTimer* power_save_timer_;
    #endif
    esp_lcd_panel_handle_t panel = nullptr;
    #ifdef LCD_GC9107
    LcdGc9107Display* display_;
    #else
    LcdDisplay* display_;
    #endif
    void InitializeSpi() {
        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num = GPIO_NUM_47;
        buscfg.miso_io_num = GPIO_NUM_NC;
        buscfg.sclk_io_num = GPIO_NUM_21;
        buscfg.quadwp_io_num = GPIO_NUM_NC;
        buscfg.quadhd_io_num = GPIO_NUM_NC;
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
        ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO));
    }

    #ifdef POWER_CONFIG
    void InitializePowerManager() {
        power_manager_ = new PowerManager(POWER_IO);
        power_manager_->OnChargingStatusChanged([this](bool is_charging) {
            if (is_charging) {
                power_save_timer_->SetEnabled(false);
            } else {
                power_save_timer_->SetEnabled(true);
            }
        });
    }

    void InitializePowerSaveTimer() {
        power_save_timer_ = new PowerSaveTimer(-1, 150, 300);
        power_save_timer_->OnEnterSleepMode([this]() {
            ESP_LOGI(TAG, "Enabling sleep mode");
            display_->SetChatMessage("system", "");
            display_->SetEmotion("sleepy");
            display_->SetFace("sleepy");
            GetBacklight()->SetBrightness(1);
        });
        power_save_timer_->OnExitSleepMode([this]() {
            display_->SetChatMessage("system", "");
            display_->SetEmotion("neutral");
            display_->SetFace("neutral");
            GetBacklight()->RestoreBrightness();
        });
        power_save_timer_->OnShutdownRequest([this]() {
            ESP_LOGI(TAG, "Shutting down");
            esp_lcd_panel_disp_on_off(panel, false); //关闭显示
            // esp_deep_sleep_start();
        });
        power_save_timer_->SetEnabled(true);
    }
    #endif

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            #ifdef POWER_CONFIG
            power_save_timer_->WakeUp();
            #endif
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting && !WifiStation::GetInstance().IsConnected()) {
                ResetWifiConfiguration();
            }
        });
        boot_button_.OnPressDown([this]() {
            #ifdef POWER_CONFIG
            power_save_timer_->WakeUp();
            #endif
            Application::GetInstance().StartListening();
        });
        boot_button_.OnPressUp([this]() {
            #ifdef POWER_CONFIG
            power_save_timer_->WakeUp();
            #endif
            Application::GetInstance().StopListening();
        });

        vol_up_button_.OnClick([this]() {
            #ifdef POWER_CONFIG
            power_save_timer_->WakeUp();
            #endif
            Application::GetInstance().VolUp();
        });

        vol_dn_button_.OnClick([this]() {
            #ifdef POWER_CONFIG
            power_save_timer_->WakeUp();
            #endif
            Application::GetInstance().VolDown();
        });
    }

    void InitializeGc9107Display() {
        esp_lcd_panel_io_handle_t panel_io = nullptr;
        
        // 液晶屏控制IO初始化
        ESP_LOGD(TAG, "Install panel IO");
        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = GPIO_NUM_14;
        io_config.dc_gpio_num = GPIO_NUM_45;
        io_config.spi_mode = 3;
        io_config.pclk_hz = 80 * 1000 * 1000;
        io_config.trans_queue_depth = 10;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI3_HOST, &io_config, &panel_io));

        // 初始化液晶屏驱动芯片Gc9107
        ESP_LOGD(TAG, "Install LCD driver");
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = DISPLAY_RESET_PIN;
        panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
        panel_config.bits_per_pixel = 16;
        #ifdef LCD_GC9107
        panel_config.flags.reset_active_high = 0;
        ESP_ERROR_CHECK(esp_lcd_new_panel_gc9107(panel_io, &panel_config, &panel));
        #else
        ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(panel_io, &panel_config, &panel));
        #endif
        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
        ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
        ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY));
        ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y));
        ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel, false));
        #ifdef LCD_GC9107
        display_ = new SpiLcdGc9107Display(panel_io, panel,
                            DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY,
                            {
                                .text_font = &font_Bouti_14_1,
                                .icon_font = &font_awesome_14_1,
                                .emoji_font = font_emoji_32_init(),
                            });
        #else
        display_ = new SpiLcdDisplay(panel_io, panel,
                            DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY,
                            {
                                .text_font = &font_puhui_20_4,
                                .icon_font = &font_awesome_20_4,
                                .emoji_font = font_emoji_64_init(),
                            });
        #endif
    }

    // 物联网初始化，添加对 AI 可见设备
    void InitializeIot() {
        auto& thing_manager = iot::ThingManager::GetInstance();
        thing_manager.AddThing(iot::CreateThing("Speaker"));
        // thing_manager.AddThing(iot::CreateThing("Lamp"));
        thing_manager.AddThing(iot::CreateThing("Backlight"));
        #ifdef POWER_CONFIG
        thing_manager.AddThing(iot::CreateThing("Battery"));
        #endif
    }

public:
    KEVIN_SP_V3Board() : 
    // Ml307Board(ML307_TX_PIN, ML307_RX_PIN, 4096),
    boot_button_(BOOT_BUTTON_GPIO),
    vol_up_button_(VOLUME_UP_BUTTON_GPIO),
    vol_dn_button_(VOLUME_DOWN_BUTTON_GPIO)  {
        ESP_LOGI(TAG, "Initializing KEVIN_SP_V3 Board");

        InitializeSpi();
        InitializeButtons();
        InitializeGc9107Display();
        #ifdef POWER_CONFIG  
        InitializePowerManager();
        InitializePowerSaveTimer();
        #endif
        InitializeIot();
        GetBacklight()->RestoreBrightness();
    }
    

    // virtual Led* GetLed() override {
    //     static SingleLed led(BUILTIN_LED_GPIO);
    //     return &led;
    // }

    virtual AudioCodec *GetAudioCodec() override {
        static NoAudioCodecSimplex audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_SPK_GPIO_BCLK, AUDIO_I2S_SPK_GPIO_LRCK, AUDIO_I2S_SPK_GPIO_DOUT, AUDIO_I2S_MIC_GPIO_SCK, AUDIO_I2S_MIC_GPIO_WS, AUDIO_I2S_MIC_GPIO_DIN);
        return &audio_codec;
    }

    virtual Display *GetDisplay() override {
        return display_;
    }
    
    virtual Backlight* GetBacklight() override {
        static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
        return &backlight;
    }
    #ifdef POWER_CONFIG
    virtual bool GetBatteryLevel(int& level, bool& charging, bool& discharging) override {
        static bool last_discharging = false;
        charging = power_manager_->IsCharging();
        discharging = power_manager_->IsDischarging();
        if (discharging != last_discharging) {
            power_save_timer_->SetEnabled(discharging);
            last_discharging = discharging;
        }
        level = power_manager_->GetBatteryLevel();
        return true;
    }

    virtual void SetPowerSaveMode(bool enabled) override {
        if (!enabled) {
            power_save_timer_->WakeUp();
        }
        WifiBoard::SetPowerSaveMode(enabled);
    }
    #endif
};

DECLARE_BOARD(KEVIN_SP_V3Board);
