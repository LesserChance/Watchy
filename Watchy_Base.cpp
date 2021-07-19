#include "Watchy_Base.h"

WatchyBase::WatchyBase() {}

void WatchyBase::init() {
  Wire.begin(SDA, SCL);                          // init i2c

  switch (wakeup_reason) {
    case ESP_SLEEP_WAKEUP_EXT0:  // RTC Alarm

      // Handle classical tick
      RTC.alarm(ALARM_2);  // resets the alarm flag in the RTC

      if (guiState == WATCHFACE_STATE) {
        RTC.read(currentTime);
        showWatchFace(true);  // partial updates on tick
      }
      break;

    case ESP_SLEEP_WAKEUP_EXT1:  // button Press
      handleButtonPress();
      break;

    default:  // reset
      _rtcConfig();
      _bmaConfig();
      showWatchFace(true);  // full update on reset
      break;
  }

  // Sometimes BMA crashes - simply try to reinitialize bma...
  if (sensor.getErrorCode() != 0) {
    sensor.shutDown();
    sensor.wakeUp();
    sensor.softReset();
    _bmaConfig();
  }
  deepSleep();
}

void WatchyBase::deepSleep() {
  esp_sleep_enable_ext0_wakeup(RTC_PIN,
                               0);  // enable deep sleep wake on RTC interrupt
  esp_sleep_enable_ext1_wakeup(
      EXT_INT_MASK,
      ESP_EXT1_WAKEUP_ANY_HIGH);  // enable deep sleep wake on button press
  esp_deep_sleep_start();
}

void WatchyBase::handleButtonPress() {
  uint64_t wakeupBit = esp_sleep_get_ext1_wakeup_status();

  // switch by state
  switch (guiState) {
    case WATCHFACE_STATE:
      // Menu Button
      if (wakeupBit & MENU_BTN_MASK) {
        Watchy::handleButtonPress();
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
      Watchy::handleButtonPress();
      break;
  }
}

void WatchyBase::handleBackButtonPress() {}

void WatchyBase::handleUpButtonPress() {}

void WatchyBase::handleDownButtonPress() {}

void WatchyBase::vibrate(uint8_t times, uint32_t delay_time) {
  sensor.enableFeature(BMA423_WAKEUP, false);
  vibMotor(delay_time, times * 2);
  sensor.enableFeature(BMA423_WAKEUP, true);
}

BMA423 WatchyBase::getSensor() {
  return sensor;
}

bool WatchyBase::isWifiConfigured() {
  return WIFI_CONFIGURED;
}

bool WatchyBase::isBleConfigured() {
  return BLE_CONFIGURED;
}

float WatchyBase::getBatteryVoltage(){
    return analogRead(ADC_PIN) / 4096.0 * 7.23;
}

void WatchyBase::_rtcConfig() {
  // https://github.com/JChristensen/DS3232RTC
  RTC.squareWave(SQWAVE_NONE);  // disable square wave output
  // RTC.set(compileTime()); //set RTC time to compile time
  RTC.setAlarm(ALM2_EVERY_MINUTE, 0, 0, 0,
               0);                    // alarm wakes up Watchy every minute
  RTC.alarmInterrupt(ALARM_2, true);  // enable alarm interrupt
  RTC.read(currentTime);
}

void WatchyBase::_bmaConfig() {
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
  // sensor.enableFeature(BMA423_WAKEUP, true);

  // Reset steps
  // sensor.resetStepCounter();

  // Turn on feature interrupt
  // sensor.enableStepCountInterrupt();
  // sensor.enableTiltInterrupt();
  // It corresponds to isDoubleClick interrupt
  // sensor.enableWakeupInterrupt();
}

uint16_t WatchyBase::_readRegister(uint8_t address, uint8_t reg, uint8_t *data,
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

uint16_t WatchyBase::_writeRegister(uint8_t address, uint8_t reg, uint8_t *data,
                                    uint16_t len) {
  Wire.beginTransmission(address);
  Wire.write(reg);
  Wire.write(data, len);
  return (0 != Wire.endTransmission());
}
