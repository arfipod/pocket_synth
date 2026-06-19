#pragma once

#include "note_map.h"
#include "synth_types.h"

namespace pocketsynth {

void sendNoteEvent(const KeyNote& note, bool pressed);
void sendWaveformEvent(Waveform waveform);
void sendVolumeDelta(float delta);

}  // namespace pocketsynth
