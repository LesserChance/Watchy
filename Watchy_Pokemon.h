#ifndef WATCHY_POKEMON_H
#define WATCHY_POKEMON_H

#include "Renderer.h"
#include "pokemon.h"

class WatchyPokemon {
    public:
        WatchyPokemon();
        void init(esp_sleep_wakeup_cause_t wakeup_reason);
        bool handleBackButtonPress();
        bool handleUpButtonPress();
        bool handleDownButtonPress();
        void drawWatchFace(tmElements_t currentTime);
};

#endif