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
  state->pitchBendMultiplier = 1.0f;
}

uint8_t activeSlotCount(const SynthAudioState& state) {
  uint8_t count = 0;
  for (const auto& note : state.notes) {
    if (note.active) ++count;
  }
  return count;
}

bool hasUiNoteIndex(uint8_t noteIndex) {
  return noteIndex < 32;
}

bool sustainPedalActiveFromCc(uint8_t value) {
  const bool active = value >= 64;
  return INVERT_SUSTAIN_PEDAL ? !active : active;
}

void noteOn(SynthAudioState* state, uint8_t noteIndex, uint8_t midi, uint8_t velocity) {
  if (state == nullptr) return;

  if (hasUiNoteIndex(noteIndex)) {
    const uint32_t bit = 1UL << noteIndex;
    if ((state->pressedMask & bit) != 0) return;
  } else {
    for (const auto& note : state->notes) {
      if (note.active && note.noteIndex == noteIndex && note.midi == midi) return;
    }
  }

  if (state->activeCount >= MAX_POLYPHONY) return;

  for (auto& note : state->notes) {
    if (note.active) continue;

    note.active = true;
    note.keyReleased = false;
    note.noteIndex = noteIndex;
    note.midi = midi;
    note.velocity = velocity;
    note.velocityGain = 0.10f + 0.90f * sqrtf(velocity / 127.0f);
    note.frequency = midiFrequency(midi);
    note.phase = 0.0f;
    note.phaseIncrement = note.frequency / static_cast<float>(SAMPLE_RATE);
    if (hasUiNoteIndex(noteIndex)) {
      state->pressedMask |= 1UL << noteIndex;
    }
    state->activeCount = activeSlotCount(*state);
    return;
  }
}

void noteOff(SynthAudioState* state, uint8_t noteIndex, uint8_t midi) {
  if (state == nullptr) return;

  if (hasUiNoteIndex(noteIndex)) {
    state->pressedMask &= ~(1UL << noteIndex);
  }

  for (auto& note : state->notes) {
    const bool matchesUiNote = hasUiNoteIndex(noteIndex) && note.noteIndex == noteIndex;
    const bool matchesExternalNote = !hasUiNoteIndex(noteIndex) && note.noteIndex == noteIndex && note.midi == midi;
    if (note.active && (matchesUiNote || matchesExternalNote)) {
      note.keyReleased = true;
      if (!state->sustainPedal) {
        note = {};
      }
    }
  }
  state->activeCount = activeSlotCount(*state);
}

void applySynthEvent(SynthAudioState* state, const SynthEvent& event) {
  if (state == nullptr) return;

  switch (event.type) {
    case SynthEventType::NoteOn:
      noteOn(state, event.noteIndex, event.midi, event.velocity);
      break;
    case SynthEventType::NoteOff:
      noteOff(state, event.noteIndex, event.midi);
      break;
    case SynthEventType::SetWaveform:
      state->waveform = event.waveform;
      break;
    case SynthEventType::AdjustVolume:
      state->masterVolume = clampFloat(state->masterVolume + event.value, 0.0f, 1.0f);
      break;
    case SynthEventType::ControlChange:
      if (event.control < 128) {
        state->cc[event.control] = event.controlValue;
      }
      if (event.control == 64) {
        state->sustainPedal = sustainPedalActiveFromCc(event.controlValue);
        if (!state->sustainPedal) {
          for (auto& note : state->notes) {
            if (note.active && note.keyReleased) {
              note = {};
            }
          }
          state->activeCount = activeSlotCount(*state);
        }
      }
      break;
    case SynthEventType::PitchBend:
      state->pitchBendRaw = event.pitchBend;
      state->pitchBendMultiplier = powf(2.0f, (static_cast<float>(event.pitchBend) / 8192.0f) * (2.0f / 12.0f));
      break;
  }
}

}  // namespace pocketsynth
