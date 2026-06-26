#pragma once

#include "synth_types.h"

#include <stdint.h>

namespace pocketsynth {

bool noteIndexHasUiKey(uint8_t noteIndex);
uint32_t uiNoteMask(uint8_t noteIndex);
uint32_t visualNoteMask(uint8_t noteIndex, uint8_t midi);
bool noteMatchesIdentity(const ActiveNote& note, uint8_t noteIndex, uint8_t midi);
float velocityToGain(uint8_t velocity);
bool sustainPedalActiveFromCc(uint8_t value);
float pitchBendMultiplierFromRaw(int16_t pitchBend);

}  // namespace pocketsynth
