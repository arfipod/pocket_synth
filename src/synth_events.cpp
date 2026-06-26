#include "synth_events.h"

#include "app_state.h"

namespace pocketsynth {

void sendNoteEvent(const KeyNote& note, bool pressed, uint8_t velocity) {
  SynthEvent event;
  event.type = pressed ? SynthEventType::NoteOn : SynthEventType::NoteOff;
  event.noteIndex = note.noteIndex;
  event.midi = note.midi;
  event.velocity = velocity;
  sendSynthEvent(event);
}

void sendMidiNoteEvent(uint8_t midi, bool pressed, uint8_t velocity) {
  SynthEvent event;
  event.type = pressed ? SynthEventType::NoteOn : SynthEventType::NoteOff;
  event.noteIndex = SYNTH_NO_UI_NOTE_INDEX;
  event.midi = midi;
  event.velocity = velocity;
  sendSynthEvent(event);
}

void sendWaveformEvent(Waveform waveform) {
  SynthEvent event = {};
  event.type = SynthEventType::SetWaveform;
  event.waveform = waveform;
  sendSynthEvent(event);
}

void sendVolumeDelta(float delta) {
  SynthEvent event;
  event.type = SynthEventType::AdjustVolume;
  event.value = delta;
  sendSynthEvent(event);
}

void sendAttackDelta(float deltaMs) {
  SynthEvent event;
  event.type = SynthEventType::AdjustAttack;
  event.value = deltaMs;
  sendSynthEvent(event);
}

void sendDecayDelta(float deltaMs) {
  SynthEvent event;
  event.type = SynthEventType::AdjustDecay;
  event.value = deltaMs;
  sendSynthEvent(event);
}

void sendSustainDelta(float delta) {
  SynthEvent event;
  event.type = SynthEventType::AdjustSustain;
  event.value = delta;
  sendSynthEvent(event);
}

void sendReleaseDelta(float deltaMs) {
  SynthEvent event;
  event.type = SynthEventType::AdjustRelease;
  event.value = deltaMs;
  sendSynthEvent(event);
}

void sendControlChangeEvent(uint8_t control, uint8_t value) {
  SynthEvent event;
  event.type = SynthEventType::ControlChange;
  event.control = control;
  event.controlValue = value;
  sendSynthEvent(event);
}

void sendPitchBendEvent(int16_t pitchBend) {
  SynthEvent event;
  event.type = SynthEventType::PitchBend;
  event.pitchBend = pitchBend;
  sendSynthEvent(event);
}

}  // namespace pocketsynth
