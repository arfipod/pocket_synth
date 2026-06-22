#include "midi_message.h"

namespace pocketsynth {
namespace {

constexpr uint8_t USB_MIDI_CIN_NOTE_OFF = 0x08;
constexpr uint8_t USB_MIDI_CIN_NOTE_ON = 0x09;
constexpr uint8_t USB_MIDI_CIN_CONTROL_CHANGE = 0x0B;
constexpr uint8_t USB_MIDI_CIN_PITCH_BEND = 0x0E;

constexpr uint8_t MIDI_STATUS_NOTE_OFF = 0x80;
constexpr uint8_t MIDI_STATUS_NOTE_ON = 0x90;
constexpr uint8_t MIDI_STATUS_CONTROL_CHANGE = 0xB0;
constexpr uint8_t MIDI_STATUS_PITCH_BEND = 0xE0;
constexpr uint8_t MIDI_STATUS_MASK = 0xF0;
constexpr uint8_t MIDI_CHANNEL_MASK = 0x0F;
constexpr int16_t MIDI_PITCH_BEND_CENTER = 8192;

constexpr bool isSevenBit(uint8_t value) {
  return value <= 0x7F;
}

constexpr MidiMessage makeChannelMessage(MidiMessageType type, uint8_t status) {
  return {type, static_cast<uint8_t>(status & MIDI_CHANNEL_MASK), 0, 0, 0, 0, 0};
}

constexpr MidiMessage parseUsbMidiPacketValue(const uint8_t packet[4]) {
  const uint8_t cin = packet[0] & 0x0F;
  const uint8_t status = packet[1];
  const uint8_t statusType = status & MIDI_STATUS_MASK;

  if (!isSevenBit(packet[2]) || !isSevenBit(packet[3])) {
    return makeUnknownMidiMessage();
  }

  if (cin == USB_MIDI_CIN_NOTE_OFF && statusType == MIDI_STATUS_NOTE_OFF) {
    MidiMessage message = makeChannelMessage(MidiMessageType::NoteOff, status);
    message.note = packet[2];
    message.velocity = packet[3];
    return message;
  }

  if (cin == USB_MIDI_CIN_NOTE_ON && statusType == MIDI_STATUS_NOTE_ON) {
    MidiMessage message = makeChannelMessage(packet[3] == 0 ? MidiMessageType::NoteOff : MidiMessageType::NoteOn,
                                             status);
    message.note = packet[2];
    message.velocity = packet[3];
    return message;
  }

  if (cin == USB_MIDI_CIN_CONTROL_CHANGE && statusType == MIDI_STATUS_CONTROL_CHANGE) {
    MidiMessage message = makeChannelMessage(MidiMessageType::ControlChange, status);
    message.control = packet[2];
    message.value = packet[3];
    return message;
  }

  if (cin == USB_MIDI_CIN_PITCH_BEND && statusType == MIDI_STATUS_PITCH_BEND) {
    const int16_t raw = static_cast<int16_t>(packet[2] | (packet[3] << 7));
    MidiMessage message = makeChannelMessage(MidiMessageType::PitchBend, status);
    message.pitchBend = static_cast<int16_t>(raw - MIDI_PITCH_BEND_CENTER);
    return message;
  }

  return makeUnknownMidiMessage();
}

constexpr uint8_t kNoteOnPacket[4] = {0x09, 0x90, 0x3C, 0x64};
constexpr uint8_t kNoteOffPacket[4] = {0x08, 0x80, 0x3C, 0x00};
constexpr uint8_t kNoteOnZeroPacket[4] = {0x09, 0x90, 0x3C, 0x00};
constexpr uint8_t kControlChangePacket[4] = {0x0B, 0xB0, 0x40, 0x7F};
constexpr uint8_t kPitchBendCenterPacket[4] = {0x0E, 0xE0, 0x00, 0x40};

static_assert(parseUsbMidiPacketValue(kNoteOnPacket).type == MidiMessageType::NoteOn, "Note On type");
static_assert(parseUsbMidiPacketValue(kNoteOnPacket).note == 60, "Note On note");
static_assert(parseUsbMidiPacketValue(kNoteOnPacket).velocity == 100, "Note On velocity");
static_assert(parseUsbMidiPacketValue(kNoteOffPacket).type == MidiMessageType::NoteOff, "Note Off type");
static_assert(parseUsbMidiPacketValue(kNoteOffPacket).note == 60, "Note Off note");
static_assert(parseUsbMidiPacketValue(kNoteOnZeroPacket).type == MidiMessageType::NoteOff, "Note On zero type");
static_assert(parseUsbMidiPacketValue(kNoteOnZeroPacket).note == 60, "Note On zero note");
static_assert(parseUsbMidiPacketValue(kControlChangePacket).type == MidiMessageType::ControlChange,
              "Control Change type");
static_assert(parseUsbMidiPacketValue(kControlChangePacket).control == 64, "Control Change control");
static_assert(parseUsbMidiPacketValue(kControlChangePacket).value == 127, "Control Change value");
static_assert(parseUsbMidiPacketValue(kPitchBendCenterPacket).type == MidiMessageType::PitchBend,
              "Pitch Bend type");
static_assert(parseUsbMidiPacketValue(kPitchBendCenterPacket).pitchBend == 0, "Pitch Bend center");

}  // namespace

MidiMessage parseUsbMidiPacket(const uint8_t* packet) {
  if (packet == nullptr) {
    return makeUnknownMidiMessage();
  }
  return parseUsbMidiPacketValue(packet);
}

}  // namespace pocketsynth
