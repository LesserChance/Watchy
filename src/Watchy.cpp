#include "Watchy.h"

DS3232RTC Watchy::RTC(false);
GxEPD2_BW<GxEPD2_154_D67, GxEPD2_154_D67::HEIGHT> Watchy::display(
    GxEPD2_154_D67(CS, DC, RESET, BUSY));

RTC_DATA_ATTR int guiState;
RTC_DATA_ATTR int menuIndex;
RTC_DATA_ATTR BMA423 sensor;
RTC_DATA_ATTR bool WIFI_CONFIGURED;
RTC_DATA_ATTR bool BLE_CONFIGURED;
RTC_DATA_ATTR weatherData currentWeather;
RTC_DATA_ATTR int weatherIntervalCounter = WEATHER_UPDATE_INTERVAL;

String getValue(String data, char separator, int index) {
  int found = 0;
  int strIndex[] = {0, -1};
  int maxIndex = data.length() - 1;

  for (int i = 0; i <= maxIndex && found <= index; i++) {
    if (data.charAt(i) == separator || i == maxIndex) {
      found++;
      strIndex[0] = strIndex[1] + 1;
      strIndex[1] = (i == maxIndex) ? i + 1 : i;
    }
  }

  return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}

Watchy::Watchy() {}  // constructor

void Watchy::init(String datetime) {
  esp_sleep_wakeup_cause_t wakeup_reason;
  wakeup_reason = esp_sleep_get_wakeup_cause();  // get wake up reason
  Wire.begin(SDA, SCL);                          // init i2c

  switch (wakeup_reason) {
#ifdef ESP_RTC
    case ESP_SLEEP_WAKEUP_TIMER:  // ESP Internal RTC
      if (guiState == WATCHFACE_STATE) {
        RTC.read(currentTime);
        currentTime.Minute++;
        tmElements_t tm;
        tm.Month = currentTime.Month;
        tm.Day = currentTime.Day;
        tm.Year = currentTime.Year;
        tm.Hour = currentTime.Hour;
        tm.Minute = currentTime.Minute;
        tm.Second = 0;
        time_t t = makeTime(tm);
        RTC.set(t);
        RTC.read(currentTime);
        showWatchFace(true);  // partial updates on tick
      }
      break;
#endif
    case ESP_SLEEP_WAKEUP_EXT0:  // RTC Alarm
      RTC.alarm(ALARM_2);        // resets the alarm flag in the RTC
      if (guiState == WATCHFACE_STATE) {
        RTC.read(currentTime);
        showWatchFace(true);  // partial updates on tick
      }
      break;
    case ESP_SLEEP_WAKEUP_EXT1:  // button Press
      handleButtonPress();
      break;
    default:  // reset
#ifndef ESP_RTC
      _rtcConfig(datetime);
#endif
      _bmaConfig();
      showWatchFace(false);  // full update on reset
      break;
  }
  deepSleep();
}

void Watchy::deepSleep() {
#ifndef ESP_RTC
  esp_sleep_enable_ext0_wakeup(RTC_PIN,
                               0);  // enable deep sleep wake on RTC interrupt
#endif
#ifdef ESP_RTC
  esp_sleep_enable_timer_wakeup(60000000);
#endif
  esp_sleep_enable_ext1_wakeup(
      BTN_PIN_MASK,
      ESP_EXT1_WAKEUP_ANY_HIGH);  // enable deep sleep wake on button press
  esp_deep_sleep_start();
}

float Watchy::getBatteryVoltage() {
  return analogRead(ADC_PIN) / 4096.0 * 7.23;
}

void Watchy::vibMotor(uint8_t intervalMs, uint8_t length) {
  pinMode(VIB_MOTOR_PIN, OUTPUT);
  bool motorOn = false;
  for (int i = 0; i < length; i++) {
    motorOn = !motorOn;
    digitalWrite(VIB_MOTOR_PIN, motorOn);
    delay(intervalMs);
  }
}

void Watchy::_rtcConfig(String datetime) {
  if (datetime != NULL) {
    const time_t FUDGE(30);  // fudge factor to allow for upload time, etc. (seconds, YMMV)
    tmElements_t tm;
    tm.Year = getValue(datetime, ':', 0).toInt() -
              YEAR_OFFSET;  // offset from 1970, since year is stored in uint8_t
    tm.Month = getValue(datetime, ':', 1).toInt();
    tm.Day = getValue(datetime, ':', 2).toInt();
    tm.Hour = getValue(datetime, ':', 3).toInt();
    tm.Minute = getValue(datetime, ':', 4).toInt();
    tm.Second = getValue(datetime, ':', 5).toInt();

    time_t t = makeTime(tm) + FUDGE;
    RTC.set(t);
  }
  // https://github.com/JChristensen/DS3232RTC
  RTC.squareWave(SQWAVE_NONE);  // disable square wave output
  // RTC.set(compileTime()); //set RTC time to compile time
  RTC.setAlarm(ALM2_EVERY_MINUTE, 0, 0, 0,
               0);                    // alarm wakes up Watchy every minute
  RTC.alarmInterrupt(ALARM_2, true);  // enable alarm interrupt
  RTC.read(currentTime);
}

void Watchy::_handleButtonPressInMenu(uint64_t wakeupBit) {
  // Menu Button
  if (wakeupBit & MENU_BTN_MASK) {
    // if already in menu, then select menu item
    if (guiState == MAIN_MENU_STATE) {
      switch (menuIndex) {
        case 0:
          showBattery();
          break;
        case 1:
          showBuzz();
          break;
        case 2:
          showAccelerometer();
          break;
        case 3:
          setTime();
          break;
        case 4:
          setupWifi();
          break;
        case 5:
          showUpdateFW();
          break;
        default:
          break;
      }
    } else if (guiState == FW_UPDATE_STATE) {
      updateFWBegin();
    }
  }

  // Back Button
  else if (wakeupBit & BACK_BTN_MASK) {
    if (guiState == MAIN_MENU_STATE) {
      // reset the alarm flag in the RTC
      RTC.alarm(ALARM_2);
      RTC.read(currentTime);
      showWatchFace(false);
    } else if (guiState == APP_STATE) {
      _drawMenu(menuIndex, false);
    } else if (guiState == FW_UPDATE_STATE) {
      _drawMenu(menuIndex, false);
    }
  }

  // Up Button
  else if (wakeupBit & UP_BTN_MASK) {
    if (guiState == MAIN_MENU_STATE) {
      menuIndex--;
      if (menuIndex < 0) {
        menuIndex = MENU_LENGTH - 1;
      }
      _drawMenu(menuIndex, true);
    }
  }

  // Down Button
  else if (wakeupBit & DOWN_BTN_MASK) {
    if (guiState == MAIN_MENU_STATE) {
      menuIndex++;
      if (menuIndex > MENU_LENGTH - 1) {
        menuIndex = 0;
      }
      _drawMenu(menuIndex, true);
    }
  }

  /**
   * Fast menu runs a loop for five seconds to immediately respond to
   * button presses while we are assumed to be in the menu
   */
  _fastMenu();
}

void Watchy::_fastMenu() {
  bool timeout = false;
  long lastTimeout = millis();
  pinMode(MENU_BTN_PIN, INPUT);
  pinMode(BACK_BTN_PIN, INPUT);
  pinMode(UP_BTN_PIN, INPUT);
  pinMode(DOWN_BTN_PIN, INPUT);
  while (!timeout) {
    if (millis() - lastTimeout > 5000) {
      timeout = true;
    } else {
      if (digitalRead(MENU_BTN_PIN) == 1) {
        lastTimeout = millis();
        if (guiState == MAIN_MENU_STATE) {  // if already in menu, then select menu item
          switch (menuIndex) {
            case 0:
              showBattery();
              break;
            case 1:
              showBuzz();
              break;
            case 2:
              showAccelerometer();
              break;
            case 3:
              setTime();
              break;
            case 4:
              setupWifi();
              break;
            case 5:
              showUpdateFW();
              break;
            default:
              break;
          }
        } else if (guiState == FW_UPDATE_STATE) {
          updateFWBegin();
        }
      } else if (digitalRead(BACK_BTN_PIN) == 1) {
        lastTimeout = millis();
        if (guiState == MAIN_MENU_STATE) {  // exit to watch face if already in menu
          RTC.alarm(ALARM_2);   // resets the alarm flag in the RTC
          RTC.read(currentTime);
          showWatchFace(false);
          break;  // leave loop
        } else if (guiState == APP_STATE) {
          _drawMenu(menuIndex, false);  // exit to menu if already in app
        } else if (guiState == FW_UPDATE_STATE) {
          _drawMenu(menuIndex, false);  // exit to menu if already in app
        }
      } else if (digitalRead(UP_BTN_PIN) == 1) {
        lastTimeout = millis();
        if (guiState == MAIN_MENU_STATE) {  // increment menu index
          menuIndex--;
          if (menuIndex < 0) {
            menuIndex = MENU_LENGTH - 1;
          }
          _showFastMenu(menuIndex);
        }
      } else if (digitalRead(DOWN_BTN_PIN) == 1) {
        lastTimeout = millis();
        if (guiState == MAIN_MENU_STATE) {  // decrement menu index
          menuIndex++;
          if (menuIndex > MENU_LENGTH - 1) {
            menuIndex = 0;
          }
          _showFastMenu(menuIndex);
        }
      }
    }
  }
}

void Watchy::_handleButtonPress() {
  uint64_t wakeupBit = esp_sleep_get_ext1_wakeup_status();

  // switch by state
  switch (guiState) {
    case WATCHFACE_STATE:
      // Menu Button
      if (wakeupBit & MENU_BTN_MASK) {
        showMenu();
      }

      // Back Button
      else if (wakeupBit & BACK_BTN_MASK) {
        handleBackButtonPress();
      }

      // Up Button
      else if (wakeupBit & UP_BTN_MASK) {
        handleUpButtonPress();
      }

      // Down Button
      else if (wakeupBit & DOWN_BTN_MASK) {
        handleDownButtonPress();
      }
      break;

    case MAIN_MENU_STATE:
    case APP_STATE:
    case FW_UPDATE_STATE:
      _handleButtonPressInMenu(wakeupBit);
      break;
  }

  display.hibernate();
}

void Watchy::_drawMenu(byte menuIndex, bool partialRefresh) {
  display.fillScreen(GxEPD_BLACK);
  display.setFont(&FreeMonoBold9pt7b);

  int16_t x1, y1;
  uint16_t w, h;
  int16_t yPos;

  const char *menuItems[] = {"Check Battery",      "Vibrate Motor",
                             "Show Accelerometer", "Set Time",
                             "Setup WiFi",         "Update Firmware"};
  for (int i = 0; i < MENU_LENGTH; i++) {
    yPos = 30 + (MENU_HEIGHT * i);
    display.setCursor(0, yPos);
    if (i == menuIndex) {
      display.getTextBounds(menuItems[i], 0, yPos, &x1, &y1, &w, &h);
      display.fillRect(x1 - 1, y1 - 10, 200, h + 15, GxEPD_WHITE);
      display.setTextColor(GxEPD_BLACK);
      display.println(menuItems[i]);
    } else {
      display.setTextColor(GxEPD_WHITE);
      display.println(menuItems[i]);
    }
  }

  display.display(partialRefresh);
}

void Watchy::_showFastMenu(byte menuIndex) {
  display.setFullWindow();
  display.fillScreen(GxEPD_BLACK);
  display.setFont(&FreeMonoBold9pt7b);

  int16_t x1, y1;
  uint16_t w, h;
  int16_t yPos;

  const char *menuItems[] = {"Check Battery",      "Vibrate Motor",
                             "Show Accelerometer", "Set Time",
                             "Setup WiFi",         "Update Firmware"};
  for (int i = 0; i < MENU_LENGTH; i++) {
    yPos = 30 + (MENU_HEIGHT * i);
    display.setCursor(0, yPos);
    if (i == menuIndex) {
      display.getTextBounds(menuItems[i], 0, yPos, &x1, &y1, &w, &h);
      display.fillRect(x1 - 1, y1 - 10, 200, h + 15, GxEPD_WHITE);
      display.setTextColor(GxEPD_BLACK);
      display.println(menuItems[i]);
    } else {
      display.setTextColor(GxEPD_WHITE);
      display.println(menuItems[i]);
    }
  }

  partialRefreshDisplay();
  guiState = MAIN_MENU_STATE;
}

void Watchy::showBattery() {
  initBlackScreen();

  display.setFont(&FreeMonoBold9pt7b);
  display.setTextColor(GxEPD_WHITE);
  display.setCursor(20, 30);
  display.println("Battery Voltage:");

  float voltage = getBatteryVoltage();
  display.setCursor(70, 80);
  display.print(voltage);
  display.println("V");

  refreshAndHibernate();

  guiState = APP_STATE;
}

void Watchy::showBuzz() {
  initBlackScreen();

  display.setFont(&FreeMonoBold9pt7b);
  display.setTextColor(GxEPD_WHITE);
  display.setCursor(70, 80);
  display.println("Buzz!");

  refreshAndHibernate();
  
  vibMotor();
  _drawMenu(menuIndex, false);
}

void Watchy::setTime() {
  guiState = APP_STATE;

  RTC.read(currentTime);

  int8_t minute = currentTime.Minute;
  int8_t hour = currentTime.Hour;
  int8_t day = currentTime.Day;
  int8_t month = currentTime.Month;
  int8_t year = currentTime.Year + YEAR_OFFSET - 2000;

  int8_t setIndex = SET_HOUR;

  int8_t blink = 0;

  pinMode(DOWN_BTN_PIN, INPUT);
  pinMode(UP_BTN_PIN, INPUT);
  pinMode(MENU_BTN_PIN, INPUT);
  pinMode(BACK_BTN_PIN, INPUT);

  display.init( 0, true);  
  display.setFullWindow();

  while (1) {
    if (digitalRead(MENU_BTN_PIN) == 1) {
      setIndex++;
      if (setIndex > SET_DAY) {
        break;
      }
    }
    if (digitalRead(BACK_BTN_PIN) == 1) {
      if (setIndex != SET_HOUR) {
        setIndex--;
      }
    }

    blink = 1 - blink;

    if (digitalRead(DOWN_BTN_PIN) == 1) {
      blink = 1;
      switch (setIndex) {
        case SET_HOUR:
          hour == 23 ? (hour = 0) : hour++;
          break;
        case SET_MINUTE:
          minute == 59 ? (minute = 0) : minute++;
          break;
        case SET_YEAR:
          year == 99 ? (year = 20) : year++;
          break;
        case SET_MONTH:
          month == 12 ? (month = 1) : month++;
          break;
        case SET_DAY:
          day == 31 ? (day = 1) : day++;
          break;
        default:
          break;
      }
    }

    if (digitalRead(UP_BTN_PIN) == 1) {
      blink = 1;
      switch (setIndex) {
        case SET_HOUR:
          hour == 0 ? (hour = 23) : hour--;
          break;
        case SET_MINUTE:
          minute == 0 ? (minute = 59) : minute--;
          break;
        case SET_YEAR:
          year == 20 ? (year = 99) : year--;
          break;
        case SET_MONTH:
          month == 1 ? (month = 12) : month--;
          break;
        case SET_DAY:
          day == 1 ? (day = 31) : day--;
          break;
        default:
          break;
      }
    }

    display.fillScreen(GxEPD_BLACK);
    display.setTextColor(GxEPD_WHITE);
    display.setFont(&DSEG7_Classic_Bold_53);

    display.setCursor(5, 80);
    if (setIndex == SET_HOUR) {  // blink hour digits
      display.setTextColor(blink ? GxEPD_WHITE : GxEPD_BLACK);
    }
    if (hour < 10) {
      display.print("0");
    }
    display.print(hour);

    display.setTextColor(GxEPD_WHITE);
    display.print(":");

    display.setCursor(108, 80);
    if (setIndex == SET_MINUTE) {  // blink minute digits
      display.setTextColor(blink ? GxEPD_WHITE : GxEPD_BLACK);
    }
    if (minute < 10) {
      display.print("0");
    }
    display.print(minute);

    display.setTextColor(GxEPD_WHITE);

    display.setFont(&FreeMonoBold9pt7b);
    display.setCursor(45, 150);
    if (setIndex == SET_YEAR) {  // blink minute digits
      display.setTextColor(blink ? GxEPD_WHITE : GxEPD_BLACK);
    }
    display.print(2000 + year);

    display.setTextColor(GxEPD_WHITE);
    display.print("/");

    if (setIndex == SET_MONTH) {  // blink minute digits
      display.setTextColor(blink ? GxEPD_WHITE : GxEPD_BLACK);
    }
    if (month < 10) {
      display.print("0");
    }
    display.print(month);

    display.setTextColor(GxEPD_WHITE);
    display.print("/");

    if (setIndex == SET_DAY) {  // blink minute digits
      display.setTextColor(blink ? GxEPD_WHITE : GxEPD_BLACK);
    }
    if (day < 10) {
      display.print("0");
    }
    display.print(day);
    partialRefreshDisplay();
  }

  refreshAndHibernate(true);

  storeTime(minute, hour, day, month, year);

  _drawMenu(menuIndex, false);
}

void Watchy::storeTime(int8_t minute, int8_t hour, int8_t day, month, int8_t year) {
  const time_t FUDGE(10);  // fudge factor to allow for upload time, etc. (seconds, YMMV)
  tmElements_t tm;
  tm.Month = month;
  tm.Day = day;
  tm.Year = year + 2000 - YEAR_OFFSET;  // offset from 1970, since year is stored in uint8_t
  tm.Hour = hour;
  tm.Minute = minute;
  tm.Second = 0;

  time_t t = makeTime(tm) + FUDGE;
  RTC.set(t);
}

void Watchy::showAccelerometer() {
  initBlackScreen();
  display.setFont(&FreeMonoBold9pt7b);
  display.setTextColor(GxEPD_WHITE);

  Accel acc;

  long previousMillis = 0;
  long interval = 200;

  guiState = APP_STATE;

  pinMode(BACK_BTN_PIN, INPUT);

  while (1) {
    unsigned long currentMillis = millis();

    if (digitalRead(BACK_BTN_PIN) == 1) {
      break;
    }

    if (currentMillis - previousMillis > interval) {
      previousMillis = currentMillis;
      // Get acceleration data
      bool res = sensor.getAccel(acc);
      uint8_t direction = sensor.getDirection();
      display.fillScreen(GxEPD_BLACK);
      display.setCursor(0, 30);
      if (res == false) {
        display.println("getAccel FAIL");
      } else {
        display.print("  X:");
        display.println(acc.x);
        display.print("  Y:");
        display.println(acc.y);
        display.print("  Z:");
        display.println(acc.z);

        display.setCursor(30, 130);
        switch (direction) {
          case DIRECTION_DISP_DOWN:
            display.println("FACE DOWN");
            break;
          case DIRECTION_DISP_UP:
            display.println("FACE UP");
            break;
          case DIRECTION_BOTTOM_EDGE:
            display.println("BOTTOM EDGE");
            break;
          case DIRECTION_TOP_EDGE:
            display.println("TOP EDGE");
            break;
          case DIRECTION_RIGHT_EDGE:
            display.println("RIGHT EDGE");
            break;
          case DIRECTION_LEFT_EDGE:
            display.println("LEFT EDGE");
            break;
          default:
            display.println("ERROR!!!");
            break;
        }
      }
      partialRefreshDisplay();
    }
  }

  _drawMenu(menuIndex, false);
}

void Watchy::showMenu() {
  guiState = MAIN_MENU_STATE;
  
  display.init(0, false);
  display.setFullWindow();
  _drawMenu(0, false);
  refreshAndHibernate();
}

void Watchy::showWatchFace(bool partialRefresh) {
  guiState = WATCHFACE_STATE;

  display.init(0, false);
  display.setFullWindow();
  drawWatchFace();
  refreshAndHibernate(partialRefresh);
}

void Watchy::drawWatchFace() {
  display.setFont(&DSEG7_Classic_Bold_53);
  display.setCursor(5, 53 + 60);
  if (currentTime.Hour < 10) {
    display.print("0");
  }
  display.print(currentTime.Hour);
  display.print(":");
  if (currentTime.Minute < 10) {
    display.print("0");
  }
  display.println(currentTime.Minute);
}

uint16_t Watchy::_readRegister(uint8_t address, uint8_t reg, uint8_t *data,
                               uint16_t len) {
  Wire.beginTransmission(address);
  Wire.write(reg);
  Wire.endTransmission();
  Wire.requestFrom((uint8_t)address, (uint8_t)len);
  uint8_t i = 0;
  while (Wire.available()) {
    data[i++] = Wire.read();
  }
  return 0;
}

uint16_t Watchy::_writeRegister(uint8_t address, uint8_t reg, uint8_t *data,
                                uint16_t len) {
  Wire.beginTransmission(address);
  Wire.write(reg);
  Wire.write(data, len);
  return (0 != Wire.endTransmission());
}

void Watchy::_bmaConfig() {
  if (sensor.begin(_readRegister, _writeRegister, delay) == false) {
    // fail to init BMA
    return;
  }

  // Accel parameter structure
  Acfg cfg;
  /*!
      Output data rate in Hz, Optional parameters:
          - BMA4_OUTPUT_DATA_RATE_0_78HZ
          - BMA4_OUTPUT_DATA_RATE_1_56HZ
          - BMA4_OUTPUT_DATA_RATE_3_12HZ
          - BMA4_OUTPUT_DATA_RATE_6_25HZ
          - BMA4_OUTPUT_DATA_RATE_12_5HZ
          - BMA4_OUTPUT_DATA_RATE_25HZ
          - BMA4_OUTPUT_DATA_RATE_50HZ
          - BMA4_OUTPUT_DATA_RATE_100HZ
          - BMA4_OUTPUT_DATA_RATE_200HZ
          - BMA4_OUTPUT_DATA_RATE_400HZ
          - BMA4_OUTPUT_DATA_RATE_800HZ
          - BMA4_OUTPUT_DATA_RATE_1600HZ
  */
  cfg.odr = BMA4_OUTPUT_DATA_RATE_100HZ;
  /*!
      G-range, Optional parameters:
          - BMA4_ACCEL_RANGE_2G
          - BMA4_ACCEL_RANGE_4G
          - BMA4_ACCEL_RANGE_8G
          - BMA4_ACCEL_RANGE_16G
  */
  cfg.range = BMA4_ACCEL_RANGE_2G;
  /*!
      Bandwidth parameter, determines filter configuration, Optional parameters:
          - BMA4_ACCEL_OSR4_AVG1
          - BMA4_ACCEL_OSR2_AVG2
          - BMA4_ACCEL_NORMAL_AVG4
          - BMA4_ACCEL_CIC_AVG8
          - BMA4_ACCEL_RES_AVG16
          - BMA4_ACCEL_RES_AVG32
          - BMA4_ACCEL_RES_AVG64
          - BMA4_ACCEL_RES_AVG128
  */
  cfg.bandwidth = BMA4_ACCEL_NORMAL_AVG4;

  /*! Filter performance mode , Optional parameters:
      - BMA4_CIC_AVG_MODE
      - BMA4_CONTINUOUS_MODE
  */
  cfg.perf_mode = BMA4_CONTINUOUS_MODE;

  // Configure the BMA423 accelerometer
  sensor.setAccelConfig(cfg);

  // Enable BMA423 accelerometer
  // Warning : Need to use feature, you must first enable the accelerometer
  // Warning : Need to use feature, you must first enable the accelerometer
  sensor.enableAccel();

  struct bma4_int_pin_config config;
  config.edge_ctrl = BMA4_LEVEL_TRIGGER;
  config.lvl = BMA4_ACTIVE_HIGH;
  config.od = BMA4_PUSH_PULL;
  config.output_en = BMA4_OUTPUT_ENABLE;
  config.input_en = BMA4_INPUT_DISABLE;
  // The correct trigger interrupt needs to be configured as needed
  sensor.setINTPinConfig(config, BMA4_INTR1_MAP);

  struct bma423_axes_remap remap_data;
  remap_data.x_axis = 1;
  remap_data.x_axis_sign = 0xFF;
  remap_data.y_axis = 0;
  remap_data.y_axis_sign = 0xFF;
  remap_data.z_axis = 2;
  remap_data.z_axis_sign = 0xFF;
  // Need to raise the wrist function, need to set the correct axis
  sensor.setRemapAxes(&remap_data);

  // Enable BMA423 isStepCounter feature
  sensor.enableFeature(BMA423_STEP_CNTR, true);
  // Enable BMA423 isTilt feature
  sensor.enableFeature(BMA423_TILT, true);
  // Enable BMA423 isDoubleClick feature
  sensor.enableFeature(BMA423_WAKEUP, true);

  // Reset steps
  sensor.resetStepCounter();

  // Turn on feature interrupt
  sensor.enableStepCountInterrupt();
  sensor.enableTiltInterrupt();
  // It corresponds to isDoubleClick interrupt
  sensor.enableWakeupInterrupt();
}

void Watchy::setupWifi() {
  WiFiManager wifiManager;
  wifiManager.resetSettings();
  wifiManager.setTimeout(WIFI_AP_TIMEOUT);
  wifiManager.setAPCallback(_configModeCallback);
  if (!wifiManager.autoConnect(WIFI_AP_SSID)) {  // WiFi setup failed
    initBlackScreen();
    display.setFont(&FreeMonoBold9pt7b);
    display.setTextColor(GxEPD_WHITE);
    display.setCursor(0, 30);
    display.println("Setup failed &");
    display.println("timed out!");
    refreshAndHibernate();
  } else {
    initBlackScreen();
    display.setFont(&FreeMonoBold9pt7b);
    display.setTextColor(GxEPD_WHITE);
    display.println("Connected to");
    display.println(WiFi.SSID());
    refreshAndHibernate();
  }
  // turn off radios
  WiFi.mode(WIFI_OFF);
  btStop();

  guiState = APP_STATE;
}

void Watchy::_configModeCallback(WiFiManager *myWiFiManager) {
  initBlackScreen();
  display.setFont(&FreeMonoBold9pt7b);
  display.setTextColor(GxEPD_WHITE);
  display.setCursor(0, 30);
  display.println("Connect to");
  display.print("SSID: ");
  display.println(WIFI_AP_SSID);
  display.print("IP: ");
  display.println(WiFi.softAPIP());
  refreshAndHibernate();
}

void Watchy::initBlackScreen() {
  display.init(0, false);  
  display.setFullWindow();
  display.fillScreen(GxEPD_BLACK);
}

void Watchy::refreshAndHibernate(bool partialRefresh = false) {
  if (partialRefresh) {
    partialRefreshDisplay()
  } else {
    fullRefreshDisplay();
  }
  display.hibernate();
}

void Watchy::fullRefreshDisplay() {
  display.display(false);
}
void Watchy::partialRefreshDisplay() {
  display.display(true);
}

bool Watchy::connectWiFi() {
  if (WL_CONNECT_FAILED ==
      WiFi.begin()) {  // WiFi not setup, you can also use hard coded
                       // credentials with WiFi.begin(SSID,PASS);
    WIFI_CONFIGURED = false;
  } else {
    if (WL_CONNECTED ==
        WiFi.waitForConnectResult()) {  // attempt to connect for 10s
      WIFI_CONFIGURED = true;
    } else {  // connection failed, time out
      WIFI_CONFIGURED = false;
      // turn off radios
      WiFi.mode(WIFI_OFF);
      btStop();
    }
  }
  return WIFI_CONFIGURED;
}

void Watchy::showUpdateFW() {
  initBlackScreen();
  display.setFont(&FreeMonoBold9pt7b);
  display.setTextColor(GxEPD_WHITE);
  display.setCursor(0, 30);
  display.println("Please Visit");
  display.println("watchy.sqfmi.com");
  display.println("with a Bluetooth");
  display.println("enabled device");
  display.println(" ");
  display.println("Press menu button");
  display.println("again when ready");
  display.println(" ");
  display.println("Keep USB powered");
  refreshAndHibernate();

  guiState = FW_UPDATE_STATE;
}

void Watchy::updateFWBegin() {
  initBlackScreen();
  display.setFont(&FreeMonoBold9pt7b);
  display.setTextColor(GxEPD_WHITE);
  display.setCursor(0, 30);
  display.println("Bluetooth Started");
  display.println(" ");
  display.println("Watchy BLE OTA");
  display.println(" ");
  display.println("Waiting for");
  display.println("connection...");
  fullRefreshDisplay();

  BLE BT;
  BT.begin("Watchy BLE OTA");
  int prevStatus = -1;
  int currentStatus;

  while (1) {
    currentStatus = BT.updateStatus();
    if (prevStatus != currentStatus || prevStatus == 1) {
      if (currentStatus == 0) {
        display.setFullWindow();
        display.fillScreen(GxEPD_BLACK);
        display.setFont(&FreeMonoBold9pt7b);
        display.setTextColor(GxEPD_WHITE);
        display.setCursor(0, 30);
        display.println("BLE Connected!");
        display.println(" ");
        display.println("Waiting for");
        display.println("upload...");
        fullRefreshDisplay();
      }
      if (currentStatus == 1) {
        display.setFullWindow();
        display.fillScreen(GxEPD_BLACK);
        display.setFont(&FreeMonoBold9pt7b);
        display.setTextColor(GxEPD_WHITE);
        display.setCursor(0, 30);
        display.println("Downloading");
        display.println("firmware:");
        display.println(" ");
        display.print(BT.howManyBytes());
        display.println(" bytes");
        partialRefreshDisplay();
      }
      if (currentStatus == 2) {
        display.setFullWindow();
        display.fillScreen(GxEPD_BLACK);
        display.setFont(&FreeMonoBold9pt7b);
        display.setTextColor(GxEPD_WHITE);
        display.setCursor(0, 30);
        display.println("Download");
        display.println("completed!");
        display.println(" ");
        display.println("Rebooting...");
        fullRefreshDisplay();

        delay(2000);
        esp_restart();
      }
      if (currentStatus == 4) {
        display.setFullWindow();
        display.fillScreen(GxEPD_BLACK);
        display.setFont(&FreeMonoBold9pt7b);
        display.setTextColor(GxEPD_WHITE);
        display.setCursor(0, 30);
        display.println("BLE Disconnected!");
        display.println(" ");
        display.println("exiting...");
        fullRefreshDisplay();
        delay(1000);
        break;
      }
      prevStatus = currentStatus;
    }
    delay(100);
  }

  // turn off radios
  WiFi.mode(WIFI_OFF);
  btStop();
  _drawMenu(menuIndex, false);
}