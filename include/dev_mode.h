#pragma once

#include "esp_err.h"

namespace pocketsynth {

bool isDevModeBuildEnabled();
bool isDevModeForced();
bool isDevModeActive();
esp_err_t initializeDevMode();

}  // namespace pocketsynth
