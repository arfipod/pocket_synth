#pragma once

#include "esp_err.h"

namespace pocketsynth {

bool isUsbHostRuntimeBuildEnabled();
esp_err_t initializeUsbHostRuntime();

}  // namespace pocketsynth
