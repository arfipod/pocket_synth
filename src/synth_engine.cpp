#include "synth_engine.h"

#include "synth_note_policy.h"

#include <math.h>

namespace pocketsynth {
namespace {

ActiveNote* findFreeNoteSlot(SynthAudioState& state) {
  for (auto& note : state.notes) {
    if (!note.active) return &note;
  }
  return nullptr;
}

bool noteAlreadyStarted(const SynthAudioState& state, uint8_t noteIndex, uint8_t midi) {
  const uint32_t mask = uiNoteMask(noteIndex);
  if (mask != 0) {
    return (state.pressedMask & mask) != 0;
  }

  for (const auto& note : state.notes) {
    if (note.active && noteMatchesIdentity(note, noteIndex, midi)) return true;
  }
  return false;
}

void refreshActiveCount(SynthAudioState& state) {
  state.activeCount = activeSlotCount(state);
}

void clearReleasedSustainNotes(SynthAudioState& state) {
  for (auto& note : state.notes) {
    if (note.active && note.keyReleased) {
      note = {};
    }
  }
  refreshActiveCount(state);
}

void startNoteInSlot(ActiveNote& note, uint8_t noteIndex, uint8_t midi, uint8_t velocity) {
  note = {};
  note.active = true;
  note.noteIndex = noteIndex;
  note.midi = midi;
  note.velocity = velocity;
  note.velocityGain = velocityToGain(velocity);
  note.frequency = midiFrequency(midi);
  note.phaseIncrement = note.frequency / static_cast<float>(SAMPLE_RATE);
}

void applyControlChange(SynthAudioState& state, const SynthEvent& event) {
  if (event.control < 128) {
    state.cc[event.control] = event.controlValue;
  }

  if (event.control != 64) return;

  state.sustainPedal = sustainPedalActiveFromCc(event.controlValue);
  if (!state.sustainPedal) {
    clearReleasedSustainNotes(state);
  }
}

void applyPitchBend(SynthAudioState& state, int16_t pitchBend) {
  state.pitchBendRaw = pitchBend;
  state.pitchBendMultiplier = pitchBendMultiplierFromRaw(pitchBend);
}

}  // namespace

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

float oscillatorAudioSample(float phase, Waveform waveform) {
  float sample = oscillatorSample(phase, waveform);
  switch (waveform) {
    case Waveform::Sine:
      return sample * WAVEFORM_GAIN_SINE;
    case Waveform::Square:
      return sample * WAVEFORM_GAIN_SQUARE;
    case Waveform::Rectangle:
      sample -= (2.0f * PULSE_WIDTH) - 1.0f;
      return sample * WAVEFORM_GAIN_RECTANGLE;
    case Waveform::Saw:
      return sample * WAVEFORM_GAIN_SAW;
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

void noteOn(SynthAudioState* state, uint8_t noteIndex, uint8_t midi, uint8_t velocity) {
  if (state == nullptr) return;

  if (noteAlreadyStarted(*state, noteIndex, midi)) return;
  if (activeSlotCount(*state) >= MAX_POLYPHONY) return;

  ActiveNote* note = findFreeNoteSlot(*state);
  if (note == nullptr) return;

  startNoteInSlot(*note, noteIndex, midi, velocity);
  state->pressedMask |= uiNoteMask(noteIndex);
  refreshActiveCount(*state);
}

void noteOff(SynthAudioState* state, uint8_t noteIndex, uint8_t midi) {
  if (state == nullptr) return;

  state->pressedMask &= ~uiNoteMask(noteIndex);

  for (auto& note : state->notes) {
    if (note.active && noteMatchesIdentity(note, noteIndex, midi)) {
      note.keyReleased = true;
      if (!state->sustainPedal) {
        note = {};
      }
    }
  }
  refreshActiveCount(*state);
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
      applyControlChange(*state, event);
      break;
    case SynthEventType::PitchBend:
      applyPitchBend(*state, event.pitchBend);
      break;
  }
}

}  // namespace pocketsynth
