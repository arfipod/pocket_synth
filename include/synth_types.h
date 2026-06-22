#pragma once

#include "synth_config.h"

#include <stdint.h>

namespace pocketsynth {

inline constexpr uint8_t SYNTH_NO_UI_NOTE_INDEX = 0xFF;

enum class Waveform : uint8_t {
  Sine,
  Square,
  Rectangle,
  Saw,
};

enum class SynthEventType : uint8_t {
  NoteOn,
  NoteOff,
  SetWaveform,
  AdjustVolume,
};

struct ActiveNote {
  bool active = false;
  uint8_t noteIndex = 0;
  uint8_t midi = 0;
  float frequency = 0.0f;
  float phase = 0.0f;
  float phaseIncrement = 0.0f;
};

struct SynthAudioState {
  Waveform waveform = Waveform::Sine;
  float masterVolume = INITIAL_MASTER_VOLUME;
  uint8_t activeCount = 0;
  uint32_t pressedMask = 0;
  ActiveNote notes[MAX_POLYPHONY] = {};
};

struct UiState {
  uint32_t version = 0;
  Waveform waveform = Waveform::Sine;
  float masterVolume = INITIAL_MASTER_VOLUME;
  uint8_t activeCount = 0;
  uint32_t pressedMask = 0;
  char chord[16] = "--";
};

struct SynthEvent {
  SynthEventType type = SynthEventType::NoteOn;
  uint8_t noteIndex = 0;
  uint8_t midi = 0;
  Waveform waveform = Waveform::Sine;
  float value = 0.0f;
};

}  // namespace pocketsynth
