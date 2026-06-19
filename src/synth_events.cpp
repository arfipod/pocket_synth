#include "synth_events.h"

#include "app_state.h"

namespace pocketsynth {

void sendNoteEvent(const KeyNote& note, bool pressed) {
  SynthEvent event = {};
  event.type = pressed ? SynthEventType::NoteOn : SynthEventType::NoteOff;
  event.noteIndex = note.noteIndex;
  event.midi = note.midi;
  sendSynthEvent(event);
}

void sendWaveformEvent(Waveform waveform) {
  SynthEvent event = {};
  event.type = SynthEventType::SetWaveform;
  event.waveform = waveform;
  sendSynthEvent(event);
}

void sendVolumeDelta(float delta) {
  SynthEvent event = {};
  event.type = SynthEventType::AdjustVolume;
  event.value = delta;
  sendSynthEvent(event);
}

}  // namespace pocketsynth
