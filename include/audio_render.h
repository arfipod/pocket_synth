#pragma once

#include "synth_types.h"

#include <stddef.h>
#include <stdint.h>

namespace pocketsynth {

void renderAudioBuffer(SynthAudioState* state, int32_t* buffer, size_t frames);

}  // namespace pocketsynth
