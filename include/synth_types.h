#pragma once

#include "synth_config.h"
#include "synth_envelope.h"

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
  AdjustAttack,
  AdjustDecay,
  AdjustSustain,
  AdjustRelease,
  ControlChange,
  PitchBend,
};

struct ActiveNote {
  bool active = false;
  uint8_t noteIndex = 0;
  uint8_t midi = 0;
  uint8_t velocity = 0;
  float velocityGain = 1.0f;
  float frequency = 0.0f;
  float phase = 0.0f;
  float phaseIncrement = 0.0f;
  EnvelopeState envelope = {};
  uint32_t ageSamples = 0;
  bool keyReleased = false;
};

struct SynthAudioState {
  Waveform waveform = Waveform::Sine;
  float masterVolume = INITIAL_MASTER_VOLUME;
  uint8_t activeCount = 0;
  uint32_t pressedMask = 0;
  bool sustainPedal = false;
  int16_t pitchBendRaw = 0;
  float pitchBendMultiplier = 1.0f;
  EnvelopeParams ampEnvelope = DEFAULT_AMP_ENVELOPE;
  uint8_t cc[128] = {};
  ActiveNote notes[MAX_POLYPHONY] = {};
};

struct UiState {
  uint32_t version = 0;
  Waveform waveform = Waveform::Sine;
  float masterVolume = INITIAL_MASTER_VOLUME;
  uint8_t activeCount = 0;
  uint32_t pressedMask = 0;
  float attackMs = DEFAULT_AMP_ENVELOPE.attackMs;
  float decayMs = DEFAULT_AMP_ENVELOPE.decayMs;
  float sustainLevel = DEFAULT_AMP_ENVELOPE.sustainLevel;
  float releaseMs = DEFAULT_AMP_ENVELOPE.releaseMs;
  char chord[16] = "--";
};

struct SynthEvent {
  SynthEventType type = SynthEventType::NoteOn;
  uint8_t noteIndex = 0;
  uint8_t midi = 0;
  uint8_t velocity = 0;
  Waveform waveform = Waveform::Sine;
  float value = 0.0f;
  uint8_t control = 0;
  uint8_t controlValue = 0;
  int16_t pitchBend = 0;
};

}  // namespace pocketsynth
