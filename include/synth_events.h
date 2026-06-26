#pragma once

#include "note_map.h"
#include "synth_types.h"

namespace pocketsynth {

void sendNoteEvent(const KeyNote& note, bool pressed, uint8_t velocity);
void sendMidiNoteEvent(uint8_t midi, bool pressed, uint8_t velocity);
void sendWaveformEvent(Waveform waveform);
void sendVolumeDelta(float delta);
void sendAttackDelta(float deltaMs);
void sendDecayDelta(float deltaMs);
void sendSustainDelta(float delta);
void sendReleaseDelta(float deltaMs);
void sendControlChangeEvent(uint8_t control, uint8_t value);
void sendPitchBendEvent(int16_t pitchBend);

}  // namespace pocketsynth
