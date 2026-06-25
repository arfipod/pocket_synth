#pragma once

#include "esp_err.h"

#include <stddef.h>
#include <stdint.h>

namespace pocketsynth {

esp_err_t ensureI2cBus();
esp_err_t i2cProbe(uint16_t address, int timeoutMs);
esp_err_t i2cWrite(uint16_t address, const uint8_t* data, size_t length, int timeoutMs);
esp_err_t i2cWriteRead(uint16_t address,
                       const uint8_t* writeData,
                       size_t writeLength,
                       uint8_t* readData,
                       size_t readLength,
                       int timeoutMs);

}  // namespace pocketsynth
