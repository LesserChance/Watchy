#ifndef WATCHY_FACEONE_H
#define WATCHY_FACEONE_H

#include "Renderer.h"
#include "faceone_progmem.h"

extern RTC_DATA_ATTR bool playAnim;

class FaceOne {
    public:
        FaceOne();
        void init(esp_sleep_wakeup_cause_t wakeup_reason);
        bool handleBackButtonPress();
        bool handleUpButtonPress();
        bool handleDownButtonPress();
        void drawWatchFace(tmElements_t currentTime);
        void drawAnim(tmElements_t currentTime);
};

#endif
