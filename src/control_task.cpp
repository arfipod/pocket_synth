#include "pocketsynth_tasks.h"

#include "app_state.h"
#include "chord_detector.h"
#include "synth_engine.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

namespace pocketsynth {

void controlTask(void*) {
  SynthAudioState controlState;
  initializeSynthState(&controlState);
  char chord[16] = "--";

  publishAudioState(controlState);
  publishUiFromAudioState(controlState, chord);

  for (;;) {
    SynthEvent event = {};
    if (xQueueReceive(synthEventQueue(), &event, portMAX_DELAY) != pdTRUE) continue;

    copyAudioState(&controlState);
    applySynthEvent(&controlState, event);
    detectChord(controlState, chord, sizeof(chord));
    publishAudioState(controlState);
    publishUiFromAudioState(controlState, chord);
  }
}

}  // namespace pocketsynth
