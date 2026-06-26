#include "synth_engine.h"

#include "synth_envelope.h"
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

ActiveNote* findMatchingNoteSlot(SynthAudioState& state, uint8_t noteIndex, uint8_t midi, bool keyReleased) {
  for (auto& note : state.notes) {
    if (note.active && note.keyReleased == keyReleased && noteMatchesIdentity(note, noteIndex, midi)) return &note;
  }
  return nullptr;
}

bool noteAlreadyHeld(const SynthAudioState& state, uint8_t noteIndex, uint8_t midi) {
  for (const auto& note : state.notes) {
    if (note.active && !note.keyReleased && noteMatchesIdentity(note, noteIndex, midi)) return true;
  }
  return false;
}

bool isReleaseStealCandidate(const ActiveNote& note) {
  return note.active && (note.envelope.stage == EnvelopeStage::Release || envelopeFinished(note.envelope));
}

ActiveNote* findQuietestReleaseSlot(SynthAudioState& state) {
  ActiveNote* candidate = nullptr;
  float candidateLevel = 2.0f;
  for (auto& note : state.notes) {
    if (!isReleaseStealCandidate(note)) continue;
    if (candidate == nullptr || note.envelope.level < candidateLevel) {
      candidate = &note;
      candidateLevel = note.envelope.level;
    }
  }
  return candidate;
}

ActiveNote* findOldestNoteSlot(SynthAudioState& state) {
  ActiveNote* candidate = nullptr;
  for (auto& note : state.notes) {
    if (!note.active) continue;
    if (candidate == nullptr || note.ageSamples > candidate->ageSamples) {
      candidate = &note;
    }
  }
  return candidate;
}

ActiveNote* allocateNoteSlot(SynthAudioState& state) {
  if (ActiveNote* freeSlot = findFreeNoteSlot(state)) return freeSlot;
  if (ActiveNote* releaseSlot = findQuietestReleaseSlot(state)) return releaseSlot;
  return findOldestNoteSlot(state);
}

void refreshActiveCount(SynthAudioState& state) {
  state.activeCount = activeSlotCount(state);
}

void refreshPressedMask(SynthAudioState& state) {
  uint32_t pressedMask = 0;
  for (const auto& note : state.notes) {
    if (note.active && !note.keyReleased) {
      pressedMask |= visualNoteMask(note.noteIndex, note.midi);
    }
  }
  state.pressedMask = pressedMask;
}

void releaseSustainedNotes(SynthAudioState& state) {
  for (auto& note : state.notes) {
    if (note.active && note.keyReleased) {
      envelopeNoteOff(note.envelope);
    }
  }
  refreshActiveCount(state);
  refreshPressedMask(state);
}

void startNoteInSlot(ActiveNote& note, uint8_t noteIndex, uint8_t midi, uint8_t velocity) {
  note = {};
  note.active = true;
  note.noteIndex = noteIndex;
  note.midi = midi;
  note.velocity = velocity;
  note.velocityGain = velocityToGain(velocity);
  note.frequency = midiFrequency(midi);
  note.phase = 0.0f;
  note.phaseIncrement = note.frequency / static_cast<float>(SAMPLE_RATE);
  note.keyReleased = false;
  note.ageSamples = 0;
  envelopeNoteOn(note.envelope);
}

void adjustAttack(SynthAudioState& state, float deltaMs) {
  state.ampEnvelope.attackMs =
      clampFloat(state.ampEnvelope.attackMs + deltaMs, ENVELOPE_ATTACK_MIN_MS, ENVELOPE_ATTACK_MAX_MS);
}

void adjustDecay(SynthAudioState& state, float deltaMs) {
  state.ampEnvelope.decayMs =
      clampFloat(state.ampEnvelope.decayMs + deltaMs, ENVELOPE_DECAY_MIN_MS, ENVELOPE_DECAY_MAX_MS);
}

void adjustSustain(SynthAudioState& state, float delta) {
  state.ampEnvelope.sustainLevel =
      clampFloat(state.ampEnvelope.sustainLevel + delta, ENVELOPE_SUSTAIN_MIN_LEVEL, ENVELOPE_SUSTAIN_MAX_LEVEL);
}

void adjustRelease(SynthAudioState& state, float deltaMs) {
  state.ampEnvelope.releaseMs =
      clampFloat(state.ampEnvelope.releaseMs + deltaMs, ENVELOPE_RELEASE_MIN_MS, ENVELOPE_RELEASE_MAX_MS);
}

void applyControlChange(SynthAudioState& state, const SynthEvent& event) {
  if (event.control < 128) {
    state.cc[event.control] = event.controlValue;
  }

  if (event.control != 64) return;

  state.sustainPedal = sustainPedalActiveFromCc(event.controlValue);
  if (!state.sustainPedal) {
    releaseSustainedNotes(state);
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
  state->ampEnvelope = DEFAULT_AMP_ENVELOPE;
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

  if (noteAlreadyHeld(*state, noteIndex, midi)) return;

  ActiveNote* note = findMatchingNoteSlot(*state, noteIndex, midi, true);
  if (note == nullptr) {
    note = allocateNoteSlot(*state);
  }
  if (note == nullptr) return;

  startNoteInSlot(*note, noteIndex, midi, velocity);
  refreshActiveCount(*state);
  refreshPressedMask(*state);
}

void noteOff(SynthAudioState* state, uint8_t noteIndex, uint8_t midi) {
  if (state == nullptr) return;

  for (auto& note : state->notes) {
    if (note.active && noteMatchesIdentity(note, noteIndex, midi)) {
      note.keyReleased = true;
      if (!state->sustainPedal) {
        envelopeNoteOff(note.envelope);
      }
    }
  }
  refreshActiveCount(*state);
  refreshPressedMask(*state);
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
    case SynthEventType::AdjustAttack:
      adjustAttack(*state, event.value);
      break;
    case SynthEventType::AdjustDecay:
      adjustDecay(*state, event.value);
      break;
    case SynthEventType::AdjustSustain:
      adjustSustain(*state, event.value);
      break;
    case SynthEventType::AdjustRelease:
      adjustRelease(*state, event.value);
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
