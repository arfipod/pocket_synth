#pragma once

#include "synth_types.h"

#include <stdint.h>

namespace pocketsynth {

float clampFloat(float value, float low, float high);
float midiFrequency(uint8_t midi);
int pitchClassForMidi(uint8_t midi);
const char* waveformShortName(Waveform waveform);
float oscillatorSample(float phase, Waveform waveform);
int16_t floatToI16(float sample);

void initializeSynthState(SynthAudioState* state);
uint8_t activeSlotCount(const SynthAudioState& state);
void noteOn(SynthAudioState* state, uint8_t noteIndex, uint8_t midi, uint8_t velocity);
void noteOff(SynthAudioState* state, uint8_t noteIndex, uint8_t midi);
void applySynthEvent(SynthAudioState* state, const SynthEvent& event);

}  // namespace pocketsynth
