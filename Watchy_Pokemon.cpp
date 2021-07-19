#include "Watchy_Pokemon.h"

WatchyPokemon::WatchyPokemon(){} //constructor

void WatchyPokemon::init(esp_sleep_wakeup_cause_t wakeup_reason) {
  
}

bool WatchyPokemon::handleBackButtonPress() {
  return false;
}

bool WatchyPokemon::handleUpButtonPress() {
  return false;
}

bool WatchyPokemon::handleDownButtonPress() {
  return false;
}

void WatchyPokemon::drawWatchFace(tmElements_t currentTime){
    Renderer::display.fillScreen(GxEPD_WHITE);
    Renderer::display.drawBitmap(0, 0, pokemon, DISPLAY_WIDTH, DISPLAY_HEIGHT, GxEPD_BLACK);
    Renderer::display.setTextColor(GxEPD_BLACK);
    Renderer::display.setFont(&FreeMonoBold9pt7b);
    Renderer::display.setCursor(10, 170);

    int Hour = currentTime.Hour;

    // Change to 12 Hour Time
    if (Renderer::isTwelveMode()) {
      if (Hour > 12) {
        Hour = Hour - 12;
      } else if (Hour == 0) {
        Hour = 12;
      }
    }
    
    if(Hour < 10){
        Renderer::display.print('0');
    }
    Renderer::display.print(Hour);
    Renderer::display.print(':');
    if(currentTime.Minute < 10){
        Renderer::display.print('0');
    }    
    Renderer::display.print(currentTime.Minute);
}