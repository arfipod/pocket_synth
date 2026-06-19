#include "synth_engine.h"

#include <math.h>

namespace pocketsynth {

float clampFloat(float value, float low, float high) {
  if (value < low) return low;
  if (value > high) return high;
  return value;
}

float midiFrequency(uint8_t midi) {
  return 440.0f * powf(2.0f, (static_cast<float>(midi) - 69.0f) / 12.0f);
}

int pitchClassForMidi(uint8_t midi) {
  return midi % 12;
}

const char* waveformShortName(Waveform waveform) {
  switch (waveform) {
    case Waveform::Sine:
      return "SIN";
    case Waveform::Square:
      return "SQR";
    case Waveform::Rectangle:
      return "RECT";
    case Waveform::Saw:
      return "SAW";
  }
  return "?";
}

float oscillatorSample(float phase, Waveform waveform) {
  switch (waveform) {
    case Waveform::Sine:
      return sinf(phase * TWO_PI);
    case Waveform::Square:
      return phase < 0.5f ? 1.0f : -1.0f;
    case Waveform::Rectangle:
      return phase < PULSE_WIDTH ? 1.0f : -1.0f;
    case Waveform::Saw:
      return 2.0f * phase - 1.0f;
  }
  return 0.0f;
}

int16_t floatToI16(float sample) {
  sample = clampFloat(sample, -1.0f, 1.0f);
  return static_cast<int16_t>(sample * 32767.0f);
}

void initializeSynthState(SynthAudioState* state) {
  if (state == nullptr) return;
  *state = {};
  state->waveform = Waveform::Sine;
  state->masterVolume = INITIAL_MASTER_VOLUME;
}

uint8_t activeSlotCount(const SynthAudioState& state) {
  uint8_t count = 0;
  for (const auto& note : state.notes) {
    if (note.active) ++count;
  }
  return count;
}

void noteOn(SynthAudioState* state, uint8_t noteIndex, uint8_t midi) {
  if (state == nullptr) return;

  const uint32_t bit = 1UL << noteIndex;
  if ((state->pressedMask & bit) != 0 || state->activeCount >= MAX_POLYPHONY) return;

  for (auto& note : state->notes) {
    if (note.active) continue;

    note.active = true;
    note.noteIndex = noteIndex;
    note.midi = midi;
    note.frequency = midiFrequency(midi);
    note.phase = 0.0f;
    note.phaseIncrement = note.frequency / static_cast<float>(SAMPLE_RATE);
    state->pressedMask |= bit;
    state->activeCount = activeSlotCount(*state);
    return;
  }
}

void noteOff(SynthAudioState* state, uint8_t noteIndex) {
  if (state == nullptr) return;

  const uint32_t bit = 1UL << noteIndex;
  state->pressedMask &= ~bit;
  for (auto& note : state->notes) {
    if (note.active && note.noteIndex == noteIndex) {
      note = {};
    }
  }
  state->activeCount = activeSlotCount(*state);
}

void applySynthEvent(SynthAudioState* state, const SynthEvent& event) {
  if (state == nullptr) return;

  switch (event.type) {
    case SynthEventType::NoteOn:
      noteOn(state, event.noteIndex, event.midi);
      break;
    case SynthEventType::NoteOff:
      noteOff(state, event.noteIndex);
      break;
    case SynthEventType::SetWaveform:
      state->waveform = event.waveform;
      break;
    case SynthEventType::AdjustVolume:
      state->masterVolume = clampFloat(state->masterVolume + event.value, 0.0f, 1.0f);
      break;
  }
}

}  // namespace pocketsynth
