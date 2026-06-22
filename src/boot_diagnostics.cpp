#include "boot_diagnostics.h"

#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

namespace pocketsynth {
namespace {

constexpr size_t LOG_ENTRY_COUNT = 32;
constexpr size_t LOG_ENTRY_LENGTH = 128;

struct DiagnosticLogEntry {
  uint32_t sequence;
  char text[LOG_ENTRY_LENGTH];
};

portMUX_TYPE gDiagnosticsMux = portMUX_INITIALIZER_UNLOCKED;
BootDiagnosticsSnapshot gSnapshot = {};
DiagnosticLogEntry gLogEntries[LOG_ENTRY_COUNT] = {};
uint32_t gNextLogSequence = 1;
size_t gLogWriteIndex = 0;
size_t gLogCount = 0;

void copyString(char* out, size_t outSize, const char* in) {
  if (out == nullptr || outSize == 0) {
    return;
  }
  snprintf(out, outSize, "%s", in ? in : "");
}

}  // namespace

void resetBootDiagnostics() {
  portENTER_CRITICAL(&gDiagnosticsMux);
  gSnapshot = {};
  gSnapshot.i2cResult = ESP_ERR_INVALID_STATE;
  gSnapshot.i2sResult = ESP_ERR_INVALID_STATE;
  gSnapshot.codecResult = ESP_ERR_INVALID_STATE;
  gNextLogSequence = 1;
  gLogWriteIndex = 0;
  gLogCount = 0;
  for (auto& entry : gLogEntries) {
    entry.sequence = 0;
    entry.text[0] = '\0';
  }
  portEXIT_CRITICAL(&gDiagnosticsMux);
}

void setBootDiagnosticResult(DiagnosticStep step, esp_err_t result) {
  portENTER_CRITICAL(&gDiagnosticsMux);
  switch (step) {
    case DiagnosticStep::I2c:
      gSnapshot.i2cResult = result;
      break;
    case DiagnosticStep::I2s:
      gSnapshot.i2sResult = result;
      break;
    case DiagnosticStep::Codec:
      gSnapshot.codecResult = result;
      break;
    case DiagnosticStep::AppState:
      gSnapshot.appStateReady = result == ESP_OK;
      break;
    case DiagnosticStep::TaskProbe:
      gSnapshot.taskProbeReady = result == ESP_OK;
      break;
  }
  portEXIT_CRITICAL(&gDiagnosticsMux);
}

void setBootDiagnosticFlag(DiagnosticStep step, bool ok) {
  setBootDiagnosticResult(step, ok ? ESP_OK : ESP_FAIL);
}

void setBootSelfTestState(bool pendingOtaVerification, bool coreSelfTestPassed) {
  portENTER_CRITICAL(&gDiagnosticsMux);
  gSnapshot.pendingOtaVerification = pendingOtaVerification;
  gSnapshot.coreSelfTestPassed = coreSelfTestPassed;
  portEXIT_CRITICAL(&gDiagnosticsMux);
}

void copyBootDiagnostics(BootDiagnosticsSnapshot* out) {
  if (out == nullptr) {
    return;
  }
  portENTER_CRITICAL(&gDiagnosticsMux);
  *out = gSnapshot;
  portEXIT_CRITICAL(&gDiagnosticsMux);
}

void addDiagnosticLog(const char* level, const char* component, const char* format, ...) {
  char message[80] = {};
  va_list args;
  va_start(args, format);
  vsnprintf(message, sizeof(message), format ? format : "", args);
  va_end(args);

  char line[LOG_ENTRY_LENGTH] = {};
  const uint32_t uptimeMs = static_cast<uint32_t>(esp_timer_get_time() / 1000ULL);
  snprintf(line,
           sizeof(line),
           "%lu [%s] %s: %s",
           static_cast<unsigned long>(uptimeMs),
           level ? level : "I",
           component ? component : "diag",
           message);

  portENTER_CRITICAL(&gDiagnosticsMux);
  DiagnosticLogEntry& entry = gLogEntries[gLogWriteIndex];
  entry.sequence = gNextLogSequence++;
  copyString(entry.text, sizeof(entry.text), line);
  gLogWriteIndex = (gLogWriteIndex + 1) % LOG_ENTRY_COUNT;
  if (gLogCount < LOG_ENTRY_COUNT) {
    ++gLogCount;
  }
  portEXIT_CRITICAL(&gDiagnosticsMux);
}

size_t copyDiagnosticLogs(char* out, size_t outSize) {
  if (out == nullptr || outSize == 0) {
    return 0;
  }

  size_t used = 0;
  size_t count = 0;
  size_t start = 0;
  portENTER_CRITICAL(&gDiagnosticsMux);
  count = gLogCount;
  start = (gLogWriteIndex + LOG_ENTRY_COUNT - gLogCount) % LOG_ENTRY_COUNT;
  portEXIT_CRITICAL(&gDiagnosticsMux);

  for (size_t i = 0; i < count && used + 1 < outSize; ++i) {
    DiagnosticLogEntry entry = {};
    portENTER_CRITICAL(&gDiagnosticsMux);
    entry = gLogEntries[(start + i) % LOG_ENTRY_COUNT];
    portEXIT_CRITICAL(&gDiagnosticsMux);

    const int written = snprintf(out + used,
                                 outSize - used,
                                 "#%lu %s\n",
                                 static_cast<unsigned long>(entry.sequence),
                                 entry.text);
    if (written <= 0) {
      break;
    }
    const size_t advanced = static_cast<size_t>(written);
    if (advanced >= outSize - used) {
      used = outSize - 1;
      break;
    }
    used += advanced;
  }
  out[used] = '\0';
  return used;
}

}  // namespace pocketsynth
