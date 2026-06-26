#pragma once

#include "synth_config.h"

#include <stdint.h>

namespace pocketsynth {

enum class EnvelopeStage : uint8_t {
  Off,
  Attack,
  Decay,
  Sustain,
  Release,
};

struct EnvelopeParams {
  float attackMs = 5.0f;
  float decayMs = 80.0f;
  float sustainLevel = 0.65f;
  float releaseMs = 120.0f;
};

struct EnvelopeState {
  EnvelopeStage stage = EnvelopeStage::Off;
  float level = 0.0f;
  float releaseStartLevel = 0.0f;
  uint32_t sampleInStage = 0;
};

inline constexpr float ENVELOPE_ATTACK_MIN_MS = 0.0f;
inline constexpr float ENVELOPE_ATTACK_MAX_MS = 3000.0f;
inline constexpr float ENVELOPE_DECAY_MIN_MS = 0.0f;
inline constexpr float ENVELOPE_DECAY_MAX_MS = 3000.0f;
inline constexpr float ENVELOPE_SUSTAIN_MIN_LEVEL = 0.0f;
inline constexpr float ENVELOPE_SUSTAIN_MAX_LEVEL = 1.0f;
inline constexpr float ENVELOPE_RELEASE_MIN_MS = 0.0f;
inline constexpr float ENVELOPE_RELEASE_MAX_MS = 5000.0f;

inline constexpr float ENVELOPE_TIME_STEP_MS = 10.0f;
inline constexpr float ENVELOPE_SUSTAIN_STEP = 0.05f;

inline constexpr EnvelopeParams DEFAULT_AMP_ENVELOPE = {
    5.0f,
    80.0f,
    0.65f,
    120.0f,
};

void envelopeNoteOn(EnvelopeState& env);
void envelopeNoteOff(EnvelopeState& env);
float envelopeNextGain(EnvelopeState& env, const EnvelopeParams& params);
bool envelopeFinished(const EnvelopeState& env);
const char* envelopeStageName(EnvelopeStage stage);

}  // namespace pocketsynth
