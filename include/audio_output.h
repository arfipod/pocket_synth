#pragma once

#include "esp_err.h"

#include <stddef.h>
#include <stdint.h>

namespace pocketsynth {

esp_err_t initializeI2sOutput();
esp_err_t initializeCodecOutput();
void writeAudioFrames(const int32_t* buffer, size_t frames);

}  // namespace pocketsynth
