#include "FaceOne.h"
#include <stdlib.h>  //srand, rand

const unsigned char *dk_nums[10] = {dk0, dk1, dk2, dk3, dk4,
                                    dk5, dk6, dk7, dk8, dk9};
const unsigned char *dk_anim1[2] = {dk_1, dk_2};
const unsigned char *dk_anim2[2] = {dk_3, dk_4};
const unsigned char *dk_anim3[4] = {dk_5, dk_6, dk_7, dk_8};

RTC_DATA_ATTR bool playAnim = false;

FaceOne::FaceOne() {}  // constructor

void FaceOne::init(esp_sleep_wakeup_cause_t wakeup_reason) {
  switch (wakeup_reason) {
    case ESP_SLEEP_WAKEUP_EXT0: //RTC Alarm
      if (Renderer::isWatchfaceState()) {
        playAnim = true;
      }
      break;
  }
}

bool FaceOne::handleBackButtonPress() {
  return true;
}

bool FaceOne::handleUpButtonPress() {
  playAnim = true;
  return true;
}

bool FaceOne::handleDownButtonPress() {
  playAnim = true;
  return true;
}

void FaceOne::drawWatchFace(tmElements_t currentTime) {
  int Hour = currentTime.Hour;

  // Change to 12 Hour Time
  if (Renderer::isTwelveMode()) {
    if (Hour > 12) {
      Hour = Hour - 12;
    } else if (Hour == 0) {
      Hour = 12;
    }
  }

  // BG
  Renderer::display.fillScreen(GxEPD_BLACK);
  Renderer::display.drawBitmap(0, 0, bg, DISPLAY_WIDTH, DISPLAY_HEIGHT, GxEPD_WHITE);

  // Hour
  Renderer::display.drawBitmap(35, 33, dk_nums[Hour / 10], 28, 26,
                     GxEPD_WHITE);  // first digit
  Renderer::display.drawBitmap(66, 33, dk_nums[Hour % 10], 28, 26,
                     GxEPD_WHITE);  // second digit

  // Minute
  Renderer::display.drawBitmap(105, 33, dk_nums[currentTime.Minute / 10], 28, 26,
                     GxEPD_WHITE);  // first digit
  Renderer::display.drawBitmap(137, 33, dk_nums[currentTime.Minute % 10], 28, 26,
                     GxEPD_WHITE);  // second digit

  if (playAnim) {
    drawAnim(currentTime);
    playAnim = false;
  } else {
    // Static DK
    Renderer::display.fillRect(0, 73, 200, 124, GxEPD_BLACK);
    Renderer::display.drawBitmap(7, 76, dk_anim1[0], 182, 121, GxEPD_WHITE);
    Renderer::display.display(true);
  }
}

void FaceOne::drawAnim(tmElements_t currentTime) {
  // Choose an Animation
  srand(
      currentTime.Minute * currentTime.Second *
      currentTime.Wday);  // seeds rand() with the current minute causing watchy
                          // to display a new random animation once per minute
  int randNum =
      rand() % (2 + 1 - 0) + 0;  // random min 0, max 2 (3 possibilities)
  int animFrames = 2;
  int animOneFrames = 8;
  int animTwoFrames = 6;
  int animThreeFrames = 4;

  if (randNum == 0) {
    for (uint8_t i = 0; i < animOneFrames; i++) {
      Renderer::display.drawBitmap(7, 76, dk_anim1[i % animFrames], 182, 121,
                         GxEPD_WHITE);
      Renderer::display.display(true);
      Renderer::display.fillRect(0, 76, 200, 121, GxEPD_BLACK);
    }
  } else if (randNum == 1) {
    for (uint8_t i = 0; i < animTwoFrames; i++) {
      Renderer::display.drawBitmap(7, 76, dk_anim2[i % animFrames], 182, 121,
                         GxEPD_WHITE);
      Renderer::display.display(true);
      Renderer::display.fillRect(0, 76, 200, 121, GxEPD_BLACK);
    }
  } else {
    for (uint8_t i = 0; i < animThreeFrames; i++) {
      Renderer::display.drawBitmap(0, 76, dk_anim3[i % animThreeFrames], 200, 121,
                         GxEPD_WHITE);
      Renderer::display.display(true);
      Renderer::display.fillRect(0, 76, 200, 121, GxEPD_BLACK);
    }
  }

  // Hide Ghosting
  for (uint8_t i = 0; i < 3; i++) {
    Renderer::display.fillRect(0, 76, 200, 121, GxEPD_BLACK);
    Renderer::display.drawBitmap(7, 76, dk_anim1[0], 182, 121, GxEPD_WHITE);
    Renderer::display.display(true);
  }
}
