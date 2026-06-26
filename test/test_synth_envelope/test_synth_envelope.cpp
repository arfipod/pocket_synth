#include "synth_envelope.h"

#include <unity.h>

#include "../../src/synth_envelope.cpp"

using namespace pocketsynth;

extern "C" void setUp(void) {}
extern "C" void tearDown(void) {}

namespace {

float advanceEnvelope(EnvelopeState& env, const EnvelopeParams& params, uint32_t samples) {
  float gain = 0.0f;
  for (uint32_t i = 0; i < samples; ++i) {
    gain = envelopeNextGain(env, params);
  }
  return gain;
}

void assertGainInRange(float gain) {
  TEST_ASSERT_GREATER_OR_EQUAL_FLOAT(0.0f, gain);
  TEST_ASSERT_LESS_OR_EQUAL_FLOAT(1.0f, gain);
}

void assertStage(EnvelopeStage expected, EnvelopeStage actual) {
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(expected), static_cast<uint8_t>(actual));
}

}  // namespace

void test_attack_rises_to_full_level() {
  EnvelopeParams params = {10.0f, 1000.0f, 0.5f, 100.0f};
  EnvelopeState env = {};
  envelopeNoteOn(env);

  const float first = envelopeNextGain(env, params);
  float maxGain = first;
  for (uint32_t i = 0; i < 400; ++i) {
    const float gain = envelopeNextGain(env, params);
    if (gain > maxGain) maxGain = gain;
  }

  assertStage(EnvelopeStage::Decay, env.stage);
  TEST_ASSERT_LESS_THAN_FLOAT(0.05f, first);
  TEST_ASSERT_GREATER_THAN_FLOAT(0.99f, maxGain);
}

void test_decay_reaches_sustain_level() {
  EnvelopeParams params = {0.0f, 10.0f, 0.4f, 100.0f};
  EnvelopeState env = {};
  envelopeNoteOn(env);

  advanceEnvelope(env, params, 400);

  assertStage(EnvelopeStage::Sustain, env.stage);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, params.sustainLevel, env.level);
}

void test_sustain_holds_constant_level() {
  EnvelopeParams params = {0.0f, 0.0f, 0.65f, 100.0f};
  EnvelopeState env = {};
  envelopeNoteOn(env);

  const float first = envelopeNextGain(env, params);
  const float later = advanceEnvelope(env, params, 200);

  assertStage(EnvelopeStage::Sustain, env.stage);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, first, later);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, params.sustainLevel, later);
}

void test_release_reaches_off() {
  EnvelopeParams params = {0.0f, 0.0f, 0.8f, 10.0f};
  EnvelopeState env = {};
  envelopeNoteOn(env);
  envelopeNextGain(env, params);

  envelopeNoteOff(env);
  advanceEnvelope(env, params, 400);

  TEST_ASSERT_TRUE(envelopeFinished(env));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, env.level);
}

void test_zero_attack_starts_decay() {
  EnvelopeParams params = {0.0f, 50.0f, 0.5f, 100.0f};
  EnvelopeState env = {};
  envelopeNoteOn(env);

  const float gain = envelopeNextGain(env, params);

  assertStage(EnvelopeStage::Decay, env.stage);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, gain);
}

void test_zero_decay_starts_sustain() {
  EnvelopeParams params = {0.0f, 0.0f, 0.45f, 100.0f};
  EnvelopeState env = {};
  envelopeNoteOn(env);

  const float gain = envelopeNextGain(env, params);

  assertStage(EnvelopeStage::Sustain, env.stage);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, params.sustainLevel, gain);
}

void test_zero_release_starts_off() {
  EnvelopeParams params = {0.0f, 0.0f, 0.8f, 0.0f};
  EnvelopeState env = {};
  envelopeNoteOn(env);
  envelopeNextGain(env, params);

  envelopeNoteOff(env);
  const float gain = envelopeNextGain(env, params);

  TEST_ASSERT_TRUE(envelopeFinished(env));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, gain);
}

void test_gain_stays_in_range() {
  EnvelopeParams params = {3.0f, 4.0f, 1.5f, 5.0f};
  EnvelopeState env = {};
  envelopeNoteOn(env);

  for (uint32_t i = 0; i < 400; ++i) {
    assertGainInRange(envelopeNextGain(env, params));
  }
  envelopeNoteOff(env);
  for (uint32_t i = 0; i < 400; ++i) {
    assertGainInRange(envelopeNextGain(env, params));
  }
}

void test_note_off_from_attack_releases_current_level() {
  EnvelopeParams params = {1000.0f, 100.0f, 0.5f, 100.0f};
  EnvelopeState env = {};
  envelopeNoteOn(env);
  advanceEnvelope(env, params, 20);
  const float level = env.level;

  envelopeNoteOff(env);

  assertStage(EnvelopeStage::Release, env.stage);
  TEST_ASSERT_LESS_THAN_FLOAT(1.0f, env.releaseStartLevel);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, level, env.releaseStartLevel);
}

void test_note_off_from_decay_releases_current_level() {
  EnvelopeParams params = {0.0f, 1000.0f, 0.2f, 100.0f};
  EnvelopeState env = {};
  envelopeNoteOn(env);
  advanceEnvelope(env, params, 20);
  const float level = env.level;

  envelopeNoteOff(env);

  assertStage(EnvelopeStage::Release, env.stage);
  TEST_ASSERT_LESS_THAN_FLOAT(1.0f, env.releaseStartLevel);
  TEST_ASSERT_GREATER_THAN_FLOAT(params.sustainLevel, env.releaseStartLevel);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, level, env.releaseStartLevel);
}

void test_note_off_from_sustain_releases_sustain_level() {
  EnvelopeParams params = {0.0f, 0.0f, 0.7f, 100.0f};
  EnvelopeState env = {};
  envelopeNoteOn(env);
  envelopeNextGain(env, params);

  envelopeNoteOff(env);

  assertStage(EnvelopeStage::Release, env.stage);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, params.sustainLevel, env.releaseStartLevel);
}

int runEnvelopeTests() {
  UNITY_BEGIN();
  RUN_TEST(test_attack_rises_to_full_level);
  RUN_TEST(test_decay_reaches_sustain_level);
  RUN_TEST(test_sustain_holds_constant_level);
  RUN_TEST(test_release_reaches_off);
  RUN_TEST(test_zero_attack_starts_decay);
  RUN_TEST(test_zero_decay_starts_sustain);
  RUN_TEST(test_zero_release_starts_off);
  RUN_TEST(test_gain_stays_in_range);
  RUN_TEST(test_note_off_from_attack_releases_current_level);
  RUN_TEST(test_note_off_from_decay_releases_current_level);
  RUN_TEST(test_note_off_from_sustain_releases_sustain_level);
  return UNITY_END();
}

#ifdef ESP_PLATFORM
extern "C" void app_main() {
  runEnvelopeTests();
}
#else
int main(int, char**) {
  return runEnvelopeTests();
}
#endif
