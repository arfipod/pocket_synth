#pragma once

#include "synth_types.h"

#include <stddef.h>

namespace pocketsynth {

void detectChord(const SynthAudioState& state, char* out, size_t outSize);

}  // namespace pocketsynth
