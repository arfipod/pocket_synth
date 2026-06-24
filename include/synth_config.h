#pragma once

#include <stdint.h>

#ifndef POCKETSYNTH_INVERT_SUSTAIN_PEDAL
#define POCKETSYNTH_INVERT_SUSTAIN_PEDAL 0
#endif

namespace pocketsynth {

inline constexpr int SAMPLE_RATE = 22050;
inline constexpr int AUDIO_BUFFER_FRAMES = 128;
inline constexpr int MAX_POLYPHONY = 8;

inline constexpr float PI = 3.14159265358979323846f;
inline constexpr float TWO_PI = 2.0f * PI;
inline constexpr float STEP_TIME = 1.0f / static_cast<float>(SAMPLE_RATE);
inline constexpr float PER_NOTE_GAIN = 0.45f;
inline constexpr float INITIAL_MASTER_VOLUME = 0.70f;
inline constexpr float VOLUME_STEP = 0.05f;
inline constexpr float PULSE_WIDTH = 0.25f;
inline constexpr bool INVERT_SUSTAIN_PEDAL = POCKETSYNTH_INVERT_SUSTAIN_PEDAL != 0;

inline constexpr uint32_t INPUT_POLL_MS = 5;
inline constexpr uint32_t UI_FRAME_MS = 50;

inline constexpr uint32_t AUDIO_TASK_STACK = 4096;
inline constexpr uint32_t CONTROL_TASK_STACK = 4096;
inline constexpr uint32_t INPUT_TASK_STACK = 4096;
inline constexpr uint32_t UI_TASK_STACK = 8192;
inline constexpr uint32_t USB_HOST_DAEMON_TASK_STACK = 4096;
inline constexpr uint32_t USB_HOST_CLIENT_TASK_STACK = 6144;
inline constexpr uint32_t USB_MIDI_HOST_TASK_STACK = 6144;
inline constexpr uint32_t USB_M32_OLED_TASK_STACK = 6144;
inline constexpr uint32_t M32_OLED_FEEDBACK_TASK_STACK = 4096;
inline constexpr uint32_t M32_OLED_OWNERSHIP_TASK_STACK = 4096;

inline constexpr uint32_t AUDIO_TASK_PRIORITY = 20;
inline constexpr uint32_t CONTROL_TASK_PRIORITY = 9;
inline constexpr uint32_t INPUT_TASK_PRIORITY = 8;
inline constexpr uint32_t UI_TASK_PRIORITY = 4;
inline constexpr uint32_t USB_HOST_DAEMON_TASK_PRIORITY = 5;
inline constexpr uint32_t USB_HOST_CLIENT_TASK_PRIORITY = 6;
inline constexpr uint32_t USB_MIDI_HOST_TASK_PRIORITY = 7;
inline constexpr uint32_t USB_M32_OLED_TASK_PRIORITY = 7;
inline constexpr uint32_t M32_OLED_FEEDBACK_TASK_PRIORITY = 4;
inline constexpr uint32_t M32_OLED_OWNERSHIP_TASK_PRIORITY = 3;

}  // namespace pocketsynth
