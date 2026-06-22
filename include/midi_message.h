#pragma once

#include <stdint.h>

namespace pocketsynth {

enum class MidiMessageType : uint8_t {
  Unknown,
  NoteOn,
  NoteOff,
  ControlChange,
  PitchBend,
};

struct MidiMessage {
  MidiMessageType type;
  uint8_t channel;
  uint8_t note;
  uint8_t velocity;
  uint8_t control;
  uint8_t value;
  int16_t pitchBend;
};

constexpr MidiMessage makeUnknownMidiMessage() {
  return {MidiMessageType::Unknown, 0, 0, 0, 0, 0, 0};
}

MidiMessage parseUsbMidiPacket(const uint8_t* packet);

}  // namespace pocketsynth
