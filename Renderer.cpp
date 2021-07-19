#include "Renderer.h"

FaceOne faceOne;
WatchyPokemon faceTwo;
RTC_DATA_ATTR bool twelve_mode = true;
RTC_DATA_ATTR int face = 0;

Renderer::Renderer() {}  // constructor

void Renderer::init() {
  wakeup_reason = esp_sleep_get_wakeup_cause();

  if (face == 0) {
    faceOne.init(wakeup_reason);
  }
  else if (face == 1) {
    faceTwo.init(wakeup_reason);
  }

  WatchyBase::init();
}

void Renderer::drawWatchFace() {
  if (face == 0) {
    faceOne.drawWatchFace(currentTime);
  }
  else if (face == 1) {
    faceTwo.drawWatchFace(currentTime);
  }
}

void Renderer::handleBackButtonPress() {
  twelve_mode = (twelve_mode == 0) ? true : false;
  RTC.read(currentTime);
  vibrate();

  if (face == 0) {
    if (faceOne.handleBackButtonPress()) {
      showWatchFace(true);
    }
  }
  else if (face == 1) {
    if (faceTwo.handleBackButtonPress()) {
      showWatchFace(true);
    }
  }
}

void Renderer::handleUpButtonPress() {
  // todo: switch face
  RTC.read(currentTime);
  vibrate();

  if (face < 1) {
    face++;
    showWatchFace(false);
  }
}

void Renderer::handleDownButtonPress() {
  // todo: switch face
  RTC.read(currentTime);
  vibrate();

  if (face > 0) {
    face--;
    showWatchFace(false);
  }
}

bool Renderer::isTwelveMode() {
  return twelve_mode;
}

bool Renderer::isWatchfaceState() {
  return (guiState == WATCHFACE_STATE);
}