#pragma once

#include "esp_err.h"

#include <stddef.h>

namespace pocketsynth {

bool isUsbHostDiagnosticsBuildEnabled();
esp_err_t initializeUsbHostDiagnostics();
size_t writeUsbHostDiagnosticsJson(char* out, size_t outSize);

}  // namespace pocketsynth
