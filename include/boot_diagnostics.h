#pragma once

#include "esp_err.h"

#include <stddef.h>
#include <stdint.h>

namespace pocketsynth {

enum class DiagnosticStep {
  AppState,
  I2c,
  I2s,
  Codec,
  TaskProbe,
};

struct BootDiagnosticsSnapshot {
  bool appStateReady;
  bool taskProbeReady;
  bool coreSelfTestPassed;
  bool pendingOtaVerification;
  esp_err_t i2cResult;
  esp_err_t i2sResult;
  esp_err_t codecResult;
};

void resetBootDiagnostics();
void setBootDiagnosticResult(DiagnosticStep step, esp_err_t result);
void setBootDiagnosticFlag(DiagnosticStep step, bool ok);
void setBootSelfTestState(bool pendingOtaVerification, bool coreSelfTestPassed);
void copyBootDiagnostics(BootDiagnosticsSnapshot* out);

void addDiagnosticLog(const char* level, const char* component, const char* format, ...);
size_t copyDiagnosticLogs(char* out, size_t outSize);

}  // namespace pocketsynth
