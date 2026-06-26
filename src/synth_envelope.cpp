#include "synth_envelope.h"

#include <math.h>

namespace pocketsynth {
namespace {

float clampEnvelopeLevel(float value) {
  if (value < 0.0f) return 0.0f;
  if (value > 1.0f) return 1.0f;
  return value;
}

uint32_t samplesForMs(float milliseconds) {
  if (milliseconds <= 0.0f) return 0;

  const float samples = (milliseconds * static_cast<float>(SAMPLE_RATE)) / 1000.0f;
  if (samples <= 1.0f) return 1;
  return static_cast<uint32_t>(ceilf(samples));
}

void enterStage(EnvelopeState& env, EnvelopeStage stage) {
  env.stage = stage;
  env.sampleInStage = 0;
}

void enterDecay(EnvelopeState& env) {
  env.level = 1.0f;
  enterStage(env, EnvelopeStage::Decay);
}

void enterSustain(EnvelopeState& env, float sustainLevel) {
  env.level = clampEnvelopeLevel(sustainLevel);
  enterStage(env, EnvelopeStage::Sustain);
}

void enterOff(EnvelopeState& env) {
  env.stage = EnvelopeStage::Off;
  env.level = 0.0f;
  env.releaseStartLevel = 0.0f;
  env.sampleInStage = 0;
}

float attackGain(EnvelopeState& env, const EnvelopeParams& params) {
  const uint32_t durationSamples = samplesForMs(params.attackMs);
  if (durationSamples == 0) {
    enterDecay(env);
    return envelopeNextGain(env, params);
  }

  const float t = static_cast<float>(env.sampleInStage) / static_cast<float>(durationSamples);
  env.level = clampEnvelopeLevel(t);
  if (env.sampleInStage >= durationSamples) {
    enterDecay(env);
  } else {
    ++env.sampleInStage;
  }
  return env.level;
}

float decayGain(EnvelopeState& env, const EnvelopeParams& params) {
  const float sustainLevel = clampEnvelopeLevel(params.sustainLevel);
  const uint32_t durationSamples = samplesForMs(params.decayMs);
  if (durationSamples == 0) {
    enterSustain(env, sustainLevel);
    return env.level;
  }

  const float t = static_cast<float>(env.sampleInStage) / static_cast<float>(durationSamples);
  env.level = clampEnvelopeLevel(1.0f + (sustainLevel - 1.0f) * t);
  if (env.sampleInStage >= durationSamples) {
    enterSustain(env, sustainLevel);
  } else {
    ++env.sampleInStage;
  }
  return env.level;
}

float releaseGain(EnvelopeState& env, const EnvelopeParams& params) {
  const uint32_t durationSamples = samplesForMs(params.releaseMs);
  if (durationSamples == 0) {
    enterOff(env);
    return 0.0f;
  }

  const float t = static_cast<float>(env.sampleInStage) / static_cast<float>(durationSamples);
  env.level = clampEnvelopeLevel(env.releaseStartLevel * (1.0f - t));
  if (env.sampleInStage >= durationSamples) {
    enterOff(env);
  } else {
    ++env.sampleInStage;
  }
  return env.level;
}

}  // namespace

void envelopeNoteOn(EnvelopeState& env) {
  env.stage = EnvelopeStage::Attack;
  env.level = 0.0f;
  env.releaseStartLevel = 0.0f;
  env.sampleInStage = 0;
}

void envelopeNoteOff(EnvelopeState& env) {
  if (env.stage == EnvelopeStage::Off) return;
  if (env.stage == EnvelopeStage::Release) return;

  env.releaseStartLevel = clampEnvelopeLevel(env.level);
  if (env.releaseStartLevel <= 0.0f) {
    enterOff(env);
    return;
  }
  enterStage(env, EnvelopeStage::Release);
}

float envelopeNextGain(EnvelopeState& env, const EnvelopeParams& params) {
  switch (env.stage) {
    case EnvelopeStage::Off:
      enterOff(env);
      return 0.0f;
    case EnvelopeStage::Attack:
      return clampEnvelopeLevel(attackGain(env, params));
    case EnvelopeStage::Decay:
      return clampEnvelopeLevel(decayGain(env, params));
    case EnvelopeStage::Sustain:
      env.level = clampEnvelopeLevel(params.sustainLevel);
      return env.level;
    case EnvelopeStage::Release:
      return clampEnvelopeLevel(releaseGain(env, params));
  }
  enterOff(env);
  return 0.0f;
}

bool envelopeFinished(const EnvelopeState& env) {
  return env.stage == EnvelopeStage::Off;
}

const char* envelopeStageName(EnvelopeStage stage) {
  switch (stage) {
    case EnvelopeStage::Off:
      return "Off";
    case EnvelopeStage::Attack:
      return "Attack";
    case EnvelopeStage::Decay:
      return "Decay";
    case EnvelopeStage::Sustain:
      return "Sustain";
    case EnvelopeStage::Release:
      return "Release";
  }
  return "?";
}

}  // namespace pocketsynth
