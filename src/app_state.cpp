#include "app_state.h"

#include "synth_engine.h"

#include <stdio.h>

namespace pocketsynth {
namespace {

QueueHandle_t gSynthEventQueue = nullptr;
portMUX_TYPE gAudioStateMux = portMUX_INITIALIZER_UNLOCKED;
portMUX_TYPE gUiStateMux = portMUX_INITIALIZER_UNLOCKED;
SynthAudioState gAudioState;
UiState gUiState;

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

void storeRenderedPhases(const SynthAudioState& rendered) {
  portENTER_CRITICAL(&gAudioStateMux);
  for (int i = 0; i < MAX_POLYPHONY; ++i) {
    if (gAudioState.notes[i].active && rendered.notes[i].active &&
        gAudioState.notes[i].noteIndex == rendered.notes[i].noteIndex &&
        gAudioState.notes[i].midi == rendered.notes[i].midi) {
      gAudioState.notes[i].phase = rendered.notes[i].phase;
      gAudioState.notes[i].attackSamples = rendered.notes[i].attackSamples;
    }
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
  snprintf(gUiState.chord, sizeof(gUiState.chord), "%s", chord ? chord : "--");
  ++gUiState.version;
  portEXIT_CRITICAL(&gUiStateMux);
}

}  // namespace pocketsynth
