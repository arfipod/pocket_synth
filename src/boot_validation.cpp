#include "boot_validation.h"

#include "app_state.h"
#include "audio_output.h"
#include "boot_diagnostics.h"
#include "pocketsynth_tasks.h"
#include "synth_config.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#if CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE
#include "esp_ota_ops.h"
#endif

namespace pocketsynth {
namespace {

constexpr const char* TAG = "boot_validation";

void selfTestTask(void*) {
  vTaskDelay(portMAX_DELAY);
}

bool verifyRuntimeTaskCreation() {
  TaskHandle_t handles[4] = {};
  struct TaskProbe {
    const char* name;
    uint32_t stack;
    UBaseType_t priority;
    BaseType_t core;
  };

  constexpr TaskProbe probes[] = {
      {"SelfTestControl", CONTROL_TASK_STACK, CONTROL_TASK_PRIORITY, 0},
      {"SelfTestInput", INPUT_TASK_STACK, INPUT_TASK_PRIORITY, 0},
      {"SelfTestUi", UI_TASK_STACK, UI_TASK_PRIORITY, 1},
      {"SelfTestAudio", AUDIO_TASK_STACK, AUDIO_TASK_PRIORITY, 1},
  };

  for (size_t i = 0; i < sizeof(probes) / sizeof(probes[0]); ++i) {
    const TaskProbe& probe = probes[i];
    const BaseType_t ok =
        xTaskCreatePinnedToCore(selfTestTask, probe.name, probe.stack, nullptr, probe.priority, &handles[i], probe.core);
    if (ok != pdPASS || handles[i] == nullptr) {
      ESP_LOGE(TAG, "Self-test task allocation failed: %s", probe.name);
      for (TaskHandle_t handle : handles) {
        if (handle != nullptr) {
          vTaskDelete(handle);
        }
      }
      return false;
    }
  }

  for (TaskHandle_t handle : handles) {
    vTaskDelete(handle);
  }
  return true;
}

bool isPendingOtaVerification() {
#if CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE
  const esp_partition_t* running = esp_ota_get_running_partition();
  esp_ota_img_states_t state = ESP_OTA_IMG_UNDEFINED;
  esp_err_t err = esp_ota_get_state_partition(running, &state);
  if (err == ESP_OK) {
    ESP_LOGI(TAG, "Running OTA app state=%d", static_cast<int>(state));
    addDiagnosticLog("I", TAG, "running OTA state=%d", static_cast<int>(state));
    return state == ESP_OTA_IMG_PENDING_VERIFY;
  }
  if (err != ESP_ERR_NOT_SUPPORTED && err != ESP_ERR_NOT_FOUND) {
    ESP_LOGW(TAG, "OTA state read failed: %s", esp_err_to_name(err));
    addDiagnosticLog("W", TAG, "OTA state read failed: %s", esp_err_to_name(err));
  }
#endif
  return false;
}

void rollbackNow() {
#if CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE
  ESP_LOGE(TAG, "Boot self-test failed; marking app invalid and rolling back");
  addDiagnosticLog("E", TAG, "boot self-test failed; rollback requested");
  esp_err_t err = esp_ota_mark_app_invalid_rollback_and_reboot();
  ESP_LOGE(TAG, "Rollback request failed: %s", esp_err_to_name(err));
  addDiagnosticLog("E", TAG, "rollback request failed: %s", esp_err_to_name(err));
#else
  ESP_LOGE(TAG, "Boot self-test failed; rollback support is not enabled");
  addDiagnosticLog("E", TAG, "boot self-test failed; rollback unavailable");
#endif
}

}  // namespace

BootValidationResult runBootValidation() {
  BootValidationResult result = {};
  result.pendingOtaVerification = isPendingOtaVerification();
  setBootSelfTestState(result.pendingOtaVerification, false);
  if (result.pendingOtaVerification) {
    ESP_LOGW(TAG, "OTA app is pending verification; running boot self-test");
    addDiagnosticLog("W", TAG, "OTA app pending verification");
  }

  result.appStateReady = initializeAppState();
  setBootDiagnosticFlag(DiagnosticStep::AppState, result.appStateReady);
  if (!result.appStateReady) {
    ESP_LOGE(TAG, "Self-test failed: app state or event queue init failed");
    addDiagnosticLog("E", TAG, "app state/event queue init failed");
    result.coreSelfTestPassed = false;
    setBootSelfTestState(result.pendingOtaVerification, result.coreSelfTestPassed);
    if (result.pendingOtaVerification) {
      rollbackNow();
    }
    return result;
  }

  esp_err_t err = ensureI2cBus();
  setBootDiagnosticResult(DiagnosticStep::I2c, err);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "Self-test I2C init warning: %s", esp_err_to_name(err));
    addDiagnosticLog("W", TAG, "I2C init warning: %s", esp_err_to_name(err));
  } else {
    addDiagnosticLog("I", TAG, "I2C init OK");
  }

  err = initializeI2sOutput();
  setBootDiagnosticResult(DiagnosticStep::I2s, err);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "Self-test I2S init warning: %s", esp_err_to_name(err));
    addDiagnosticLog("W", TAG, "I2S init warning: %s", esp_err_to_name(err));
  } else {
    addDiagnosticLog("I", TAG, "I2S init OK");
  }

  ESP_LOGI(TAG, "Display init self-test skipped; UI task owns the display device");
  addDiagnosticLog("I", TAG, "display self-test skipped");

  result.coreSelfTestPassed = verifyRuntimeTaskCreation();
  setBootDiagnosticFlag(DiagnosticStep::TaskProbe, result.coreSelfTestPassed);

#if POCKETSYNTH_FORCE_BOOT_SELF_TEST_FAIL
  ESP_LOGE(TAG, "Forced boot self-test failure requested");
  addDiagnosticLog("E", TAG, "forced boot self-test failure requested");
  result.coreSelfTestPassed = false;
#endif

  setBootSelfTestState(result.pendingOtaVerification, result.coreSelfTestPassed);
  if (!result.coreSelfTestPassed && result.pendingOtaVerification) {
    rollbackNow();
  } else if (result.coreSelfTestPassed) {
    addDiagnosticLog("I", TAG, "boot self-test passed");
  }
  return result;
}

void confirmOtaAppIfNeeded(const BootValidationResult& result) {
  if (!result.pendingOtaVerification) {
    return;
  }

  if (!result.coreSelfTestPassed) {
    return;
  }

#if CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE
  esp_err_t err = esp_ota_mark_app_valid_cancel_rollback();
  if (err == ESP_OK) {
    ESP_LOGW(TAG, "OTA app marked valid after boot self-test");
    addDiagnosticLog("W", TAG, "OTA app marked valid");
  } else {
    ESP_LOGE(TAG, "Failed to mark OTA app valid: %s", esp_err_to_name(err));
    addDiagnosticLog("E", TAG, "mark OTA valid failed: %s", esp_err_to_name(err));
    rollbackNow();
  }
#else
  ESP_LOGW(TAG, "OTA app validation requested but rollback support is not enabled");
  addDiagnosticLog("W", TAG, "OTA validation requested but rollback unavailable");
#endif
}

}  // namespace pocketsynth
