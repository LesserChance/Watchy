#ifndef WATCHY_BASE_H
#define WATCHY_BASE_H

#include <Watchy.h>

#if __has_include("config.h") && __has_include(<stdint.h>)
# include "config.h"
#endif

#define EXT_INT_MASK        MENU_BTN_MASK|BACK_BTN_MASK|UP_BTN_MASK|DOWN_BTN_MASK

class WatchyBase : public Watchy {
    public:
        static BMA423 getSensor();
        static bool isWifiConfigured();
        static bool isBleConfigured();
        static float getBatteryVoltage();
    public:
        WatchyBase();
        virtual void init();
        virtual void handleButtonPress();
        virtual void handleBackButtonPress();
        virtual void handleUpButtonPress();
        virtual void handleDownButtonPress();
        virtual void deepSleep();
        void vibrate(uint8_t times=1, uint32_t delay_time=50);
        esp_sleep_wakeup_cause_t wakeup_reason;
    private:
        void _rtcConfig();
        void _bmaConfig();
        static uint16_t _readRegister(uint8_t address, uint8_t reg, uint8_t *data, uint16_t len);
        static uint16_t _writeRegister(uint8_t address, uint8_t reg, uint8_t *data, uint16_t len);
};

#endif
