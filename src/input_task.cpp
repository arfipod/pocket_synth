#include "pocketsynth_tasks.h"

#include "cardputer_keyboard.h"
#include "note_map.h"
#include "synth_config.h"
#include "synth_events.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace pocketsynth {
namespace {

constexpr const char* TAG = "input_task";

}  // namespace

void inputTask(void*) {
  CardputerKeyboard keyboard;
  const bool keyboardReady = keyboard.begin();
  if (!keyboardReady) {
    ESP_LOGW(TAG, "Keyboard init failed: %s", keyboard.diagnostic());
  }

  for (;;) {
    if (keyboardReady) {
      CardputerKeyEvent keyEvent = {};
      while (keyboard.readEvent(&keyEvent)) {
        if (keyEvent.key == CardputerKey::Character) {
          const KeyNote* note = findNoteByKey(keyEvent.character);
          if (note != nullptr) sendNoteEvent(*note, keyEvent.pressed);
          continue;
        }

        if (!keyEvent.pressed) continue;
        switch (keyEvent.key) {
          case CardputerKey::F1:
            sendWaveformEvent(Waveform::Sine);
            break;
          case CardputerKey::F2:
            sendWaveformEvent(Waveform::Square);
            break;
          case CardputerKey::F3:
            sendWaveformEvent(Waveform::Rectangle);
            break;
          case CardputerKey::F4:
            sendWaveformEvent(Waveform::Saw);
            break;
          case CardputerKey::Up:
            sendVolumeDelta(VOLUME_STEP);
            break;
          case CardputerKey::Down:
            sendVolumeDelta(-VOLUME_STEP);
            break;
          default:
            break;
        }
      }
    }

    vTaskDelay(pdMS_TO_TICKS(INPUT_POLL_MS));
  }
}

}  // namespace pocketsynth
