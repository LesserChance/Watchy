#ifndef WATCHY_H
#define WATCHY_H

#include <Arduino.h>
#include <Arduino_JSON.h>
#include <DS3232RTC.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include <GxEPD2_BW.h>
#include <HTTPClient.h>
#include <WiFiManager.h>
#include <Wire.h>

#include "BLE.h"
#include "DSEG7_Classic_Bold_53.h"
#include "bma.h"
#include "config.h"

typedef struct weatherData {
  int8_t temperature;
  int16_t weatherConditionCode;
} weatherData;

class Watchy {
 public:
  static DS3232RTC RTC;
  static GxEPD2_BW<GxEPD2_154_D67, GxEPD2_154_D67::HEIGHT> display;
  tmElements_t currentTime;

 public:
  Watchy();
  void init(String datetime = "");

  // Go into power saving mode, interrupted by button presses or RTC interrupt
  void deepSleep();

  // Get the current battery charge level
  float getBatteryVoltage();

  // Vibrate the internal motor
  void vibMotor(uint8_t intervalMs = 100, uint8_t length = 20);

  // show the configuration menu screen
  void showMenu();
  
  // show the watch face screen
  void showWatchFace(bool partialRefresh);

  // default Watchy menu items
  void showBattery();
  void showBuzz();
  void showAccelerometer();
  void showUpdateFW();
  void setTime();
  void setupWifi();
  bool connectWiFi();
  void updateFWBegin();

  // handy display functions
  void initBlackScreen();
  void refreshAndHibernate(bool partialRefresh = false);

  // display buffer content to screen
  void fullRefreshDisplay();
  void partialRefreshDisplay();

  // other handy functions
  void storeTime();

  // override these methods as needed
  virtual void drawWatchFace();
  virtual void handleBackButtonPress();
  virtual void handleUpButtonPress();
  virtual void handleDownButtonPress();

 private:
  void _rtcConfig(String datetime);
  void _bmaConfig();

  void _handleButtonPress();
  void _handleButtonPressInMenu(uint64_t wakeupBit);

  void _drawMenu(byte menuIndex, bool partialRefresh);
  void _fastMenu();
  void _showFastMenu(byte menuIndex);

  static void _configModeCallback(WiFiManager *myWiFiManager);
  static uint16_t _readRegister(uint8_t address, uint8_t reg, uint8_t *data,
                                uint16_t len);
  static uint16_t _writeRegister(uint8_t address, uint8_t reg, uint8_t *data,
                                 uint16_t len);
};

extern RTC_DATA_ATTR int guiState;
extern RTC_DATA_ATTR int menuIndex;
extern RTC_DATA_ATTR BMA423 sensor;
extern RTC_DATA_ATTR bool WIFI_CONFIGURED;
extern RTC_DATA_ATTR bool BLE_CONFIGURED;

#endif