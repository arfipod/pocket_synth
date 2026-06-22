#pragma once

#include "esp_err.h"

#include <stddef.h>

namespace pocketsynth {

bool isUsbMidiHostBuildEnabled();
esp_err_t initializeUsbMidiHost();
size_t writeUsbMidiHostStatusJson(char* out, size_t outSize);

}  // namespace pocketsynth
