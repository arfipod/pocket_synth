#include "app_state.h"
#include "audio_output.h"
#include "dev_mode.h"
#include "pocketsynth_tasks.h"
#include "synth_config.h"

#include "esp_err.h"
#include "esp_flash.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace {

constexpr const char* TAG = "pocketsynth";

void logDetectedFlashSize() {
  uint32_t flashSizeBytes = 0;
  esp_err_t err = esp_flash_get_size(nullptr, &flashSizeBytes);
  if (err == ESP_OK) {
    ESP_LOGI(TAG,
             "Detected flash size: %lu bytes (%lu MB)",
             static_cast<unsigned long>(flashSizeBytes),
             static_cast<unsigned long>(flashSizeBytes / (1024UL * 1024UL)));
  } else {
    ESP_LOGW(TAG, "Flash size detection failed: %s", esp_err_to_name(err));
  }
}

}  // namespace

extern "C" void app_main(void) {
  using namespace pocketsynth;

  ESP_LOGI(TAG, "Starting pocketsynth iteration 1");
  logDetectedFlashSize();
  ESP_LOGI(TAG,
           "WiFi Dev Mode build=%s forced=%s active=%s",
           isDevModeBuildEnabled() ? "yes" : "no",
           isDevModeForced() ? "yes" : "no",
           isDevModeActive() ? "yes" : "no");

  if (isDevModeActive()) {
    esp_err_t devModeErr = initializeDevMode();
    if (devModeErr != ESP_OK) {
      ESP_LOGE(TAG, "WiFi Dev Mode init failed: %s", esp_err_to_name(devModeErr));
    }
  }

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
