#include "app_state.h"
#include "audio_output.h"
#include "pocketsynth_tasks.h"
#include "synth_config.h"

#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace {

constexpr const char* TAG = "pocketsynth";

}  // namespace

extern "C" void app_main(void) {
  using namespace pocketsynth;

  ESP_LOGI(TAG, "Starting pocketsynth iteration 1");

  if (!initializeAppState()) {
    ESP_LOGE(TAG, "Synth event queue allocation failed");
    return;
  }

  esp_err_t err = ensureI2cBus();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "I2C init failed: %s", esp_err_to_name(err));
  }

  err = initializeI2sOutput();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "I2S init failed: %s", esp_err_to_name(err));
  } else {
    err = initializeCodecOutput();
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "ES8311 init failed: %s", esp_err_to_name(err));
    }
  }

  xTaskCreatePinnedToCore(controlTask, "SynthControlTask", CONTROL_TASK_STACK, nullptr, CONTROL_TASK_PRIORITY, nullptr, 0);
  xTaskCreatePinnedToCore(inputTask, "InputTask", INPUT_TASK_STACK, nullptr, INPUT_TASK_PRIORITY, nullptr, 0);
  xTaskCreatePinnedToCore(uiTask, "UiTask", UI_TASK_STACK, nullptr, UI_TASK_PRIORITY, nullptr, 1);
  xTaskCreatePinnedToCore(audioTask, "AudioTask", AUDIO_TASK_STACK, nullptr, AUDIO_TASK_PRIORITY, nullptr, 1);
}
