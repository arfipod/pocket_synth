#include "usb_host_runtime.h"

#include "boot_diagnostics.h"
#include "synth_config.h"

#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "freertos/task.h"

#ifndef POCKETSYNTH_ENABLE_USB_HOST_DIAG
#define POCKETSYNTH_ENABLE_USB_HOST_DIAG 0
#endif

#ifndef POCKETSYNTH_ENABLE_USB_MIDI_HOST
#define POCKETSYNTH_ENABLE_USB_MIDI_HOST 0
#endif

#ifndef POCKETSYNTH_ENABLE_M32_OLED
#define POCKETSYNTH_ENABLE_M32_OLED 0
#endif

#if POCKETSYNTH_ENABLE_USB_HOST_DIAG || POCKETSYNTH_ENABLE_USB_MIDI_HOST || POCKETSYNTH_ENABLE_M32_OLED
#include "esp_bit_defs.h"
#include "esp_idf_version.h"
#include "usb/usb_host.h"
#endif

namespace pocketsynth {
namespace {

constexpr const char* TAG = "usb_host";

#if POCKETSYNTH_ENABLE_USB_HOST_DIAG || POCKETSYNTH_ENABLE_USB_MIDI_HOST || POCKETSYNTH_ENABLE_M32_OLED
portMUX_TYPE gUsbHostRuntimeMux = portMUX_INITIALIZER_UNLOCKED;
bool gInitAttempted = false;
bool gHostInstalled = false;
esp_err_t gInstallResult = ESP_ERR_INVALID_STATE;

void setInstallResult(esp_err_t result) {
  portENTER_CRITICAL(&gUsbHostRuntimeMux);
  gInstallResult = result;
  gHostInstalled = result == ESP_OK;
  portEXIT_CRITICAL(&gUsbHostRuntimeMux);
}

void usbHostDaemonTask(void* notifyTaskHandle) {
  ESP_LOGI(TAG, "Installing USB Host library");

  usb_host_config_t hostConfig = {};
  hostConfig.skip_phy_setup = false;
  hostConfig.intr_flags = ESP_INTR_FLAG_LOWMED;
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0)
  hostConfig.peripheral_map = BIT0;
#endif

  const esp_err_t err = usb_host_install(&hostConfig);
  setInstallResult(err);

  if (notifyTaskHandle != nullptr) {
    xTaskNotifyGive(static_cast<TaskHandle_t>(notifyTaskHandle));
  }

  if (err != ESP_OK) {
    ESP_LOGE(TAG, "USB Host install failed: %s", esp_err_to_name(err));
    addDiagnosticLog("E", TAG, "install failed: %s", esp_err_to_name(err));
    vTaskDelete(nullptr);
    return;
  }

  ESP_LOGI(TAG, "USB Host library ready");
  addDiagnosticLog("I", TAG, "library ready");

  for (;;) {
    uint32_t eventFlags = 0;
    const esp_err_t handleErr = usb_host_lib_handle_events(portMAX_DELAY, &eventFlags);
    if (handleErr != ESP_OK) {
      ESP_LOGW(TAG, "USB Host library event handling failed: %s", esp_err_to_name(handleErr));
      addDiagnosticLog("W", TAG, "event handling failed: %s", esp_err_to_name(handleErr));
      vTaskDelay(pdMS_TO_TICKS(250));
      continue;
    }

    if ((eventFlags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) != 0) {
      ESP_LOGW(TAG, "USB Host runtime has no registered clients");
      addDiagnosticLog("W", TAG, "no registered clients");
    }
  }
}
#endif

}  // namespace

bool isUsbHostRuntimeBuildEnabled() {
#if POCKETSYNTH_ENABLE_USB_HOST_DIAG || POCKETSYNTH_ENABLE_USB_MIDI_HOST || POCKETSYNTH_ENABLE_M32_OLED
  return true;
#else
  return false;
#endif
}

esp_err_t initializeUsbHostRuntime() {
#if POCKETSYNTH_ENABLE_USB_HOST_DIAG || POCKETSYNTH_ENABLE_USB_MIDI_HOST || POCKETSYNTH_ENABLE_M32_OLED
  portENTER_CRITICAL(&gUsbHostRuntimeMux);
  if (gHostInstalled) {
    portEXIT_CRITICAL(&gUsbHostRuntimeMux);
    return ESP_OK;
  }
  if (gInitAttempted) {
    const esp_err_t result = gInstallResult;
    portEXIT_CRITICAL(&gUsbHostRuntimeMux);
    return result;
  }
  gInitAttempted = true;
  portEXIT_CRITICAL(&gUsbHostRuntimeMux);

  TaskHandle_t currentTask = xTaskGetCurrentTaskHandle();
  const BaseType_t created = xTaskCreatePinnedToCore(usbHostDaemonTask,
                                                     "UsbHostDaemon",
                                                     USB_HOST_DAEMON_TASK_STACK,
                                                     currentTask,
                                                     USB_HOST_DAEMON_TASK_PRIORITY,
                                                     nullptr,
                                                     0);
  if (created != pdTRUE) {
    setInstallResult(ESP_ERR_NO_MEM);
    return ESP_ERR_NO_MEM;
  }

  const uint32_t notified = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1500));
  if (notified == 0) {
    portENTER_CRITICAL(&gUsbHostRuntimeMux);
    if (gInstallResult == ESP_ERR_INVALID_STATE) {
      gInstallResult = ESP_ERR_TIMEOUT;
    }
    const esp_err_t result = gInstallResult;
    portEXIT_CRITICAL(&gUsbHostRuntimeMux);
    return result;
  }

  portENTER_CRITICAL(&gUsbHostRuntimeMux);
  const esp_err_t result = gInstallResult;
  portEXIT_CRITICAL(&gUsbHostRuntimeMux);
  return result;
#else
  return ESP_ERR_NOT_SUPPORTED;
#endif
}

}  // namespace pocketsynth
