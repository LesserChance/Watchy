#ifndef WATCHY_RENDERER_H
#define WATCHY_RENDERER_H

#include "Watchy_Base.h"
#include "FaceOne.h"
#include "Watchy_Pokemon.h"

extern RTC_DATA_ATTR bool twelve_mode;
extern RTC_DATA_ATTR int face;

class Renderer : public WatchyBase {
    public:
        static bool isTwelveMode();
        static bool isWatchfaceState();

    public:
        Renderer();
        void init();
        void drawWatchFace();
        void handleBackButtonPress();
        void handleUpButtonPress();
        void handleDownButtonPress();
};

#endif
