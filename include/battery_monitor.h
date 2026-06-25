#pragma once

#include <stdint.h>

namespace pocketsynth {

struct BatteryStatus {
  bool valid = false;
  uint8_t levelPercent = 0;
  uint16_t voltageMv = 0;
  bool chargingKnown = false;
  bool charging = false;
};

BatteryStatus readBatteryStatus();
bool batteryStatusEqual(const BatteryStatus& left, const BatteryStatus& right);

}  // namespace pocketsynth
