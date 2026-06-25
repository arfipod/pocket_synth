#include "battery_monitor.h"

#include "cardputer_pinmap.h"

#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_err.h"

#include <stdint.h>

namespace pocketsynth {
namespace {

constexpr adc_unit_t BATTERY_ADC_UNIT = ADC_UNIT_1;
constexpr adc_channel_t BATTERY_ADC_CHANNEL = ADC_CHANNEL_9;
constexpr adc_atten_t BATTERY_ADC_ATTEN = ADC_ATTEN_DB_12;
constexpr adc_bitwidth_t BATTERY_ADC_BITWIDTH = ADC_BITWIDTH_12;
constexpr uint32_t BATTERY_ADC_DIVIDER_RATIO_X100 = 200;
constexpr int BATTERY_EMPTY_MV = 3300;
constexpr int BATTERY_FULL_MV = 4150;
constexpr uint8_t BATTERY_ADC_SAMPLES = 8;

adc_oneshot_unit_handle_t gAdcHandle = nullptr;
adc_cali_handle_t gCaliHandle = nullptr;
bool gAdcInitAttempted = false;

bool initializeAdc() {
  if (gAdcHandle != nullptr) {
    return true;
  }
  if (gAdcInitAttempted) {
    return false;
  }
  gAdcInitAttempted = true;

  adc_unit_t unit = ADC_UNIT_1;
  adc_channel_t channel = ADC_CHANNEL_0;
  if (adc_oneshot_io_to_channel(static_cast<int>(PIN_BATTERY_ADC), &unit, &channel) == ESP_OK) {
    if (unit != BATTERY_ADC_UNIT || channel != BATTERY_ADC_CHANNEL) {
      return false;
    }
  }

  adc_oneshot_unit_init_cfg_t initConfig = {};
  initConfig.unit_id = BATTERY_ADC_UNIT;
  if (adc_oneshot_new_unit(&initConfig, &gAdcHandle) != ESP_OK || gAdcHandle == nullptr) {
    return false;
  }

  adc_oneshot_chan_cfg_t channelConfig = {};
  channelConfig.atten = BATTERY_ADC_ATTEN;
  channelConfig.bitwidth = BATTERY_ADC_BITWIDTH;
  if (adc_oneshot_config_channel(gAdcHandle, BATTERY_ADC_CHANNEL, &channelConfig) != ESP_OK) {
    (void)adc_oneshot_del_unit(gAdcHandle);
    gAdcHandle = nullptr;
    return false;
  }

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
  adc_cali_curve_fitting_config_t caliConfig = {};
  caliConfig.unit_id = BATTERY_ADC_UNIT;
  caliConfig.chan = BATTERY_ADC_CHANNEL;
  caliConfig.atten = BATTERY_ADC_ATTEN;
  caliConfig.bitwidth = BATTERY_ADC_BITWIDTH;
  (void)adc_cali_create_scheme_curve_fitting(&caliConfig, &gCaliHandle);
#elif ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
  adc_cali_line_fitting_config_t caliConfig = {};
  caliConfig.unit_id = BATTERY_ADC_UNIT;
  caliConfig.atten = BATTERY_ADC_ATTEN;
  caliConfig.bitwidth = BATTERY_ADC_BITWIDTH;
  (void)adc_cali_create_scheme_line_fitting(&caliConfig, &gCaliHandle);
#endif

  return true;
}

int pinVoltageFromRaw(int raw) {
  int calibratedMv = 0;
  if (gCaliHandle != nullptr && adc_cali_raw_to_voltage(gCaliHandle, raw, &calibratedMv) == ESP_OK) {
    return calibratedMv;
  }
  return (raw * 3300 + 2047) / 4095;
}

uint8_t batteryLevelFromVoltage(uint16_t voltageMv) {
  const int rawLevel = ((static_cast<int>(voltageMv) - BATTERY_EMPTY_MV) * 100) /
                       (BATTERY_FULL_MV - BATTERY_EMPTY_MV);
  if (rawLevel <= 0) {
    return 0;
  }
  if (rawLevel >= 100) {
    return 100;
  }
  return static_cast<uint8_t>(rawLevel);
}

}  // namespace

BatteryStatus readBatteryStatus() {
  BatteryStatus status = {};
  status.chargingKnown = false;
  status.charging = false;

  if (!initializeAdc()) {
    return status;
  }

  int64_t pinVoltageTotal = 0;
  uint8_t samples = 0;
  for (uint8_t i = 0; i < BATTERY_ADC_SAMPLES; ++i) {
    int raw = 0;
    if (adc_oneshot_read(gAdcHandle, BATTERY_ADC_CHANNEL, &raw) != ESP_OK) {
      continue;
    }
    pinVoltageTotal += pinVoltageFromRaw(raw);
    ++samples;
  }

  if (samples == 0) {
    return status;
  }

  const int pinVoltageMv = static_cast<int>((pinVoltageTotal + (samples / 2)) / samples);
  const int batteryMv = (pinVoltageMv * static_cast<int>(BATTERY_ADC_DIVIDER_RATIO_X100) + 50) / 100;
  status.valid = batteryMv > 0;
  status.voltageMv = static_cast<uint16_t>(batteryMv < 0 ? 0 : batteryMv);
  status.levelPercent = batteryLevelFromVoltage(status.voltageMv);
  return status;
}

bool batteryStatusEqual(const BatteryStatus& left, const BatteryStatus& right) {
  return left.valid == right.valid &&
         left.levelPercent == right.levelPercent &&
         left.chargingKnown == right.chargingKnown &&
         left.charging == right.charging;
}

}  // namespace pocketsynth
