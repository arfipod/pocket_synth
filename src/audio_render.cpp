#include "audio_render.h"

#include "synth_engine.h"

namespace pocketsynth {
namespace {

constexpr float INV_SQRT[MAX_POLYPHONY + 1] = {
    1.0f, 1.0f, 0.70710678f, 0.57735027f, 0.5f, 0.44721360f, 0.40824829f, 0.37796447f, 0.35355339f,
};

int32_t packI2sMonoPair(int16_t first, int16_t second) {
  return static_cast<int32_t>((static_cast<uint32_t>(static_cast<uint16_t>(first)) << 16) |
                              static_cast<uint16_t>(second));
}

float noteAttackGain(ActiveNote& note) {
  if (note.attackSamples >= NOTE_ATTACK_SAMPLES) return 1.0f;

  const float t = static_cast<float>(note.attackSamples) / static_cast<float>(NOTE_ATTACK_SAMPLES);
  ++note.attackSamples;
  return t * t * (3.0f - 2.0f * t);
}

}  // namespace

void renderAudioBuffer(SynthAudioState* state, int32_t* buffer, size_t frames) {
  if (state == nullptr || buffer == nullptr) return;

  const float normalize = state->activeCount <= MAX_POLYPHONY ? INV_SQRT[state->activeCount] : INV_SQRT[MAX_POLYPHONY];
  int16_t firstInPair = 0;

  for (size_t frame = 0; frame < frames; ++frame) {
    float mixed = 0.0f;
    if (state->activeCount > 0) {
      for (auto& note : state->notes) {
        if (!note.active) continue;

        mixed += oscillatorAudioSample(note.phase, state->waveform) * noteAttackGain(note) * PER_NOTE_GAIN *
                 note.velocityGain;
        note.phase += note.phaseIncrement * state->pitchBendMultiplier;
        if (note.phase >= 1.0f) note.phase -= 1.0f;
      }
      mixed *= normalize;
      mixed *= state->masterVolume;
    }

    const int16_t pcm = floatToI16(mixed);
    if ((frame & 1U) == 0) {
      firstInPair = pcm;
    } else {
      buffer[frame >> 1] = packI2sMonoPair(firstInPair, pcm);
    }
  }

  if ((frames & 1U) != 0) {
    buffer[frames >> 1] = packI2sMonoPair(firstInPair, firstInPair);
  }
}

}  // namespace pocketsynth
