#include "app_state.h"

#include "synth_envelope.h"
#include "synth_engine.h"

#include <stdio.h>

namespace pocketsynth {
namespace {

QueueHandle_t gSynthEventQueue = nullptr;
portMUX_TYPE gAudioStateMux = portMUX_INITIALIZER_UNLOCKED;
portMUX_TYPE gUiStateMux = portMUX_INITIALIZER_UNLOCKED;
SynthAudioState gAudioState;
UiState gUiState;

bool sameNoteIdentity(const ActiveNote& a, const ActiveNote& b) {
  return a.noteIndex == b.noteIndex && a.midi == b.midi;
}

}  // namespace

bool initializeAppState() {
  gSynthEventQueue = xQueueCreate(32, sizeof(SynthEvent));
  if (gSynthEventQueue == nullptr) return false;

  SynthAudioState initialState;
  initializeSynthState(&initialState);

  portENTER_CRITICAL(&gAudioStateMux);
  gAudioState = initialState;
  portEXIT_CRITICAL(&gAudioStateMux);

  publishUiFromAudioState(initialState, "--");
  return true;
}

QueueHandle_t synthEventQueue() {
  return gSynthEventQueue;
}

bool sendSynthEvent(const SynthEvent& event) {
  return gSynthEventQueue != nullptr && xQueueSend(gSynthEventQueue, &event, 0) == pdTRUE;
}

void copyAudioState(SynthAudioState* out) {
  if (out == nullptr) return;

  portENTER_CRITICAL(&gAudioStateMux);
  *out = gAudioState;
  portEXIT_CRITICAL(&gAudioStateMux);
}

void publishAudioState(const SynthAudioState& state) {
  portENTER_CRITICAL(&gAudioStateMux);
  gAudioState = state;
  portEXIT_CRITICAL(&gAudioStateMux);
}

void storeRenderedAudioState(const SynthAudioState& rendered) {
  portENTER_CRITICAL(&gAudioStateMux);
  bool activeCountChanged = false;
  for (int i = 0; i < MAX_POLYPHONY; ++i) {
    if (!gAudioState.notes[i].active || !sameNoteIdentity(gAudioState.notes[i], rendered.notes[i])) continue;

    if (rendered.notes[i].active) {
      if (gAudioState.notes[i].keyReleased == rendered.notes[i].keyReleased) {
        gAudioState.notes[i].phase = rendered.notes[i].phase;
        gAudioState.notes[i].envelope = rendered.notes[i].envelope;
        gAudioState.notes[i].ageSamples = rendered.notes[i].ageSamples;
      }
    } else if (gAudioState.notes[i].keyReleased || envelopeFinished(gAudioState.notes[i].envelope)) {
      gAudioState.notes[i] = {};
      activeCountChanged = true;
    }
  }
  if (activeCountChanged) {
    gAudioState.activeCount = activeSlotCount(gAudioState);
  } else if (gAudioState.activeCount != rendered.activeCount) {
    uint8_t count = 0;
    for (int i = 0; i < MAX_POLYPHONY; ++i) {
      if (gAudioState.notes[i].active) ++count;
    }
    gAudioState.activeCount = count;
  }
  portEXIT_CRITICAL(&gAudioStateMux);
}

void copyUiState(UiState* out) {
  if (out == nullptr) return;

  portENTER_CRITICAL(&gUiStateMux);
  *out = gUiState;
  portEXIT_CRITICAL(&gUiStateMux);
}

void publishUiFromAudioState(const SynthAudioState& state, const char* chord) {
  portENTER_CRITICAL(&gUiStateMux);
  gUiState.waveform = state.waveform;
  gUiState.masterVolume = state.masterVolume;
  gUiState.activeCount = state.activeCount;
  gUiState.pressedMask = state.pressedMask;
  gUiState.attackMs = state.ampEnvelope.attackMs;
  gUiState.decayMs = state.ampEnvelope.decayMs;
  gUiState.sustainLevel = state.ampEnvelope.sustainLevel;
  gUiState.releaseMs = state.ampEnvelope.releaseMs;
  snprintf(gUiState.chord, sizeof(gUiState.chord), "%s", chord ? chord : "--");
  ++gUiState.version;
  portEXIT_CRITICAL(&gUiStateMux);
}

}  // namespace pocketsynth
