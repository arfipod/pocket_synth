#pragma once

#include "synth_types.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

namespace pocketsynth {

bool initializeAppState();
QueueHandle_t synthEventQueue();

bool sendSynthEvent(const SynthEvent& event);
void copyAudioState(SynthAudioState* out);
void publishAudioState(const SynthAudioState& state);
void storeRenderedPhases(const SynthAudioState& rendered);

void copyUiState(UiState* out);
void publishUiFromAudioState(const SynthAudioState& state, const char* chord);

}  // namespace pocketsynth
