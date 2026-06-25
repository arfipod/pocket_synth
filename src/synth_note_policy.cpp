#include "synth_note_policy.h"

#include "synth_config.h"

#include <math.h>

namespace pocketsynth {
namespace {

constexpr uint8_t UI_NOTE_MASK_BITS = 32;
constexpr float MIN_VELOCITY_GAIN = 0.10f;
constexpr float MAX_VELOCITY_GAIN = 1.00f;
constexpr float PITCH_BEND_CENTER = 8192.0f;
constexpr float PITCH_BEND_RANGE_SEMITONES = 2.0f;
constexpr float SEMITONES_PER_OCTAVE = 12.0f;

}  // namespace

bool noteIndexHasUiKey(uint8_t noteIndex) {
  return noteIndex < UI_NOTE_MASK_BITS;
}

uint32_t uiNoteMask(uint8_t noteIndex) {
  return noteIndexHasUiKey(noteIndex) ? (1UL << noteIndex) : 0;
}

bool noteMatchesIdentity(const ActiveNote& note, uint8_t noteIndex, uint8_t midi) {
  if (noteIndexHasUiKey(noteIndex)) {
    return note.noteIndex == noteIndex;
  }
  return note.noteIndex == noteIndex && note.midi == midi;
}

float velocityToGain(uint8_t velocity) {
  const float normalized = static_cast<float>(velocity) / 127.0f;
  return MIN_VELOCITY_GAIN + (MAX_VELOCITY_GAIN - MIN_VELOCITY_GAIN) * sqrtf(normalized);
}

bool sustainPedalActiveFromCc(uint8_t value) {
  const bool active = value >= 64;
  return INVERT_SUSTAIN_PEDAL ? !active : active;
}

float pitchBendMultiplierFromRaw(int16_t pitchBend) {
  const float bendSemitones =
      (static_cast<float>(pitchBend) / PITCH_BEND_CENTER) * PITCH_BEND_RANGE_SEMITONES;
  return powf(2.0f, bendSemitones / SEMITONES_PER_OCTAVE);
}

}  // namespace pocketsynth
