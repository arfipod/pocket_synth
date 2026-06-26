#include "pocketsynth_tasks.h"

#include "app_state.h"
#include "chord_detector.h"
#include "synth_config.h"
#include "synth_engine.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

namespace pocketsynth {

void controlTask(void*) {
  SynthAudioState controlState;
  initializeSynthState(&controlState);
  char chord[16] = "--";
  uint8_t lastPublishedActiveCount = controlState.activeCount;
  uint32_t lastPublishedPressedMask = controlState.pressedMask;

  publishAudioState(controlState);
  publishUiFromAudioState(controlState, chord);

  for (;;) {
    SynthEvent event = {};
    if (xQueueReceive(synthEventQueue(), &event, pdMS_TO_TICKS(UI_FRAME_MS)) == pdTRUE) {
      copyAudioState(&controlState);
      applySynthEvent(&controlState, event);
      detectChord(controlState, chord, sizeof(chord));
      publishAudioState(controlState);
      publishUiFromAudioState(controlState, chord);
      lastPublishedActiveCount = controlState.activeCount;
      lastPublishedPressedMask = controlState.pressedMask;
      continue;
    }

    copyAudioState(&controlState);
    if (controlState.activeCount != lastPublishedActiveCount || controlState.pressedMask != lastPublishedPressedMask) {
      detectChord(controlState, chord, sizeof(chord));
      publishUiFromAudioState(controlState, chord);
      lastPublishedActiveCount = controlState.activeCount;
      lastPublishedPressedMask = controlState.pressedMask;
    }
  }
}

}  // namespace pocketsynth
