#pragma once

namespace pocketsynth {

struct BootValidationResult {
  bool appStateReady;
  bool coreSelfTestPassed;
  bool pendingOtaVerification;
};

BootValidationResult runBootValidation();
void confirmOtaAppIfNeeded(const BootValidationResult& result);

}  // namespace pocketsynth
