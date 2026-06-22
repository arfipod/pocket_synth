#include "app_state.h"
#include "audio_output.h"
#include "boot_diagnostics.h"
#include "boot_validation.h"
#include "dev_mode.h"
#include "pocketsynth_tasks.h"
#include "synth_config.h"
#include "usb_host_diag.h"
#include "usb_midi_host.h"

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

  resetBootDiagnostics();
  ESP_LOGI(TAG, "Starting pocketsynth iteration 1");
  addDiagnosticLog("I", TAG, "starting pocketsynth iteration 1");
  logDetectedFlashSize();
  ESP_LOGI(TAG,
           "WiFi Dev Mode build=%s forced=%s active=%s",
           isDevModeBuildEnabled() ? "yes" : "no",
           isDevModeForced() ? "yes" : "no",
           isDevModeActive() ? "yes" : "no");
  addDiagnosticLog("I",
                   TAG,
                   "WiFi Dev Mode build=%s forced=%s active=%s",
                   isDevModeBuildEnabled() ? "yes" : "no",
                   isDevModeForced() ? "yes" : "no",
                   isDevModeActive() ? "yes" : "no");
  ESP_LOGI(TAG,
           "USB Host diagnostics build=%s",
           isUsbHostDiagnosticsBuildEnabled() ? "yes" : "no");
  addDiagnosticLog("I",
                   TAG,
                   "USB Host diagnostics build=%s",
                   isUsbHostDiagnosticsBuildEnabled() ? "yes" : "no");
  ESP_LOGI(TAG,
           "USB MIDI host build=%s",
           isUsbMidiHostBuildEnabled() ? "yes" : "no");
  addDiagnosticLog("I",
                   TAG,
                   "USB MIDI host build=%s",
                   isUsbMidiHostBuildEnabled() ? "yes" : "no");

  BootValidationResult bootValidation = runBootValidation();
  if (!bootValidation.coreSelfTestPassed) {
    ESP_LOGE(TAG, "Boot self-test failed");
    addDiagnosticLog("E", TAG, "boot self-test failed");
    return;
  }
  confirmOtaAppIfNeeded(bootValidation);

  if (isDevModeActive()) {
    esp_err_t devModeErr = initializeDevMode();
    if (devModeErr != ESP_OK) {
      ESP_LOGE(TAG, "WiFi Dev Mode init failed: %s", esp_err_to_name(devModeErr));
      addDiagnosticLog("E", TAG, "WiFi Dev Mode init failed: %s", esp_err_to_name(devModeErr));
    }
  }

  if (isUsbHostDiagnosticsBuildEnabled()) {
    esp_err_t usbDiagErr = initializeUsbHostDiagnostics();
    if (usbDiagErr != ESP_OK) {
      ESP_LOGE(TAG, "USB Host diagnostics init failed: %s", esp_err_to_name(usbDiagErr));
      addDiagnosticLog("E", TAG, "USB Host diagnostics init failed: %s", esp_err_to_name(usbDiagErr));
    }
  }

  if (isUsbMidiHostBuildEnabled()) {
    esp_err_t usbMidiErr = initializeUsbMidiHost();
    if (usbMidiErr != ESP_OK) {
      ESP_LOGE(TAG, "USB MIDI host init failed: %s", esp_err_to_name(usbMidiErr));
      addDiagnosticLog("E", TAG, "USB MIDI host init failed: %s", esp_err_to_name(usbMidiErr));
    }
  }

  esp_err_t err = initializeCodecOutput();
  setBootDiagnosticResult(DiagnosticStep::Codec, err);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "ES8311 init failed: %s", esp_err_to_name(err));
    addDiagnosticLog("W", TAG, "ES8311 init failed: %s", esp_err_to_name(err));
  } else {
    addDiagnosticLog("I", TAG, "ES8311 codec init OK");
  }

  xTaskCreatePinnedToCore(controlTask, "SynthControlTask", CONTROL_TASK_STACK, nullptr, CONTROL_TASK_PRIORITY, nullptr, 0);
  xTaskCreatePinnedToCore(inputTask, "InputTask", INPUT_TASK_STACK, nullptr, INPUT_TASK_PRIORITY, nullptr, 0);
  xTaskCreatePinnedToCore(uiTask, "UiTask", UI_TASK_STACK, nullptr, UI_TASK_PRIORITY, nullptr, 1);
  xTaskCreatePinnedToCore(audioTask, "AudioTask", AUDIO_TASK_STACK, nullptr, AUDIO_TASK_PRIORITY, nullptr, 1);
}
