#include "chord_detector.h"

#include "synth_engine.h"

#include <stdio.h>
#include <stdint.h>

namespace pocketsynth {
namespace {

constexpr const char* NOTE_NAMES[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

struct ChordPattern {
  uint16_t mask;
  const char* suffix;
};

constexpr ChordPattern PATTERNS[] = {
    {static_cast<uint16_t>((1 << 0) | (1 << 4) | (1 << 8) | (1 << 11)), "Maj7#5"},
    {static_cast<uint16_t>((1 << 0) | (1 << 4) | (1 << 7) | (1 << 11)), "Maj7"},
    {static_cast<uint16_t>((1 << 0) | (1 << 3) | (1 << 7) | (1 << 10)), "m7"},
    {static_cast<uint16_t>((1 << 0) | (1 << 4) | (1 << 7) | (1 << 10)), "7"},
    {static_cast<uint16_t>((1 << 0) | (1 << 3) | (1 << 6)), "dim"},
    {static_cast<uint16_t>((1 << 0) | (1 << 4) | (1 << 8)), "aug"},
    {static_cast<uint16_t>((1 << 0) | (1 << 2) | (1 << 7)), "sus2"},
    {static_cast<uint16_t>((1 << 0) | (1 << 5) | (1 << 7)), "sus4"},
    {static_cast<uint16_t>((1 << 0) | (1 << 3) | (1 << 7)), "m"},
    {static_cast<uint16_t>((1 << 0) | (1 << 4) | (1 << 7)), ""},
};

}  // namespace

void detectChord(const SynthAudioState& state, char* out, size_t outSize) {
  if (out == nullptr || outSize == 0) return;

  snprintf(out, outSize, "--");
  if (state.activeCount < 3) return;

  uint16_t pitchMask = 0;
  uint8_t lowestMidi = 127;
  for (const auto& note : state.notes) {
    if (!note.active) continue;

    pitchMask |= static_cast<uint16_t>(1 << pitchClassForMidi(note.midi));
    if (note.midi < lowestMidi) lowestMidi = note.midi;
  }

  const int bassPc = pitchClassForMidi(lowestMidi);
  for (int root = 0; root < 12; ++root) {
    uint16_t relativeMask = 0;
    for (int pc = 0; pc < 12; ++pc) {
      if ((pitchMask & (1 << pc)) == 0) continue;

      const int interval = (pc - root + 12) % 12;
      relativeMask |= static_cast<uint16_t>(1 << interval);
    }

    for (const auto& pattern : PATTERNS) {
      if (relativeMask != pattern.mask) continue;

      if (bassPc == root) {
        snprintf(out, outSize, "%s%s", NOTE_NAMES[root], pattern.suffix);
      } else {
        snprintf(out, outSize, "%s%s/%s", NOTE_NAMES[root], pattern.suffix, NOTE_NAMES[bassPc]);
      }
      return;
    }
  }
}

}  // namespace pocketsynth
