#include "pocketsynth_tasks.h"

#include "app_state.h"
#include "cardputer_display.h"
#include "synth_config.h"
#include "synth_engine.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <math.h>
#include <stdio.h>
#include <stdint.h>

namespace pocketsynth {
namespace {

constexpr const char* TAG = "ui_task";

uint16_t colorBg() {
  return CardputerDisplay::rgb565(11, 16, 24);
}

uint16_t colorPanel() {
  return CardputerDisplay::rgb565(8, 12, 18);
}

uint16_t colorText() {
  return CardputerDisplay::rgb565(199, 215, 239);
}

uint16_t colorMuted() {
  return CardputerDisplay::rgb565(143, 160, 187);
}

uint16_t colorGreen() {
  return CardputerDisplay::rgb565(155, 255, 183);
}

uint16_t colorBlue() {
  return CardputerDisplay::rgb565(124, 199, 255);
}

uint16_t colorStroke() {
  return CardputerDisplay::rgb565(52, 68, 93);
}

void drawWaveIcon(CardputerDisplay& display, Waveform waveform, int x, int y, uint16_t color) {
  switch (waveform) {
    case Waveform::Sine:
      display.drawLine(x, y + 4, x + 2, y + 2, color);
      display.drawLine(x + 2, y + 2, x + 4, y + 1, color);
      display.drawLine(x + 4, y + 1, x + 7, y + 5, color);
      display.drawLine(x + 7, y + 5, x + 9, y + 6, color);
      display.drawLine(x + 9, y + 6, x + 12, y + 2, color);
      break;
    case Waveform::Square:
      display.drawLine(x, y + 2, x + 4, y + 2, color);
      display.drawLine(x + 4, y + 2, x + 4, y + 6, color);
      display.drawLine(x + 4, y + 6, x + 8, y + 6, color);
      display.drawLine(x + 8, y + 6, x + 8, y + 2, color);
      display.drawLine(x + 8, y + 2, x + 12, y + 2, color);
      break;
    case Waveform::Rectangle:
      display.drawLine(x, y + 2, x + 3, y + 2, color);
      display.drawLine(x + 3, y + 2, x + 3, y + 6, color);
      display.drawLine(x + 3, y + 6, x + 6, y + 6, color);
      display.drawLine(x + 6, y + 6, x + 6, y + 2, color);
      display.drawLine(x + 6, y + 2, x + 12, y + 2, color);
      break;
    case Waveform::Saw:
      display.drawLine(x, y + 6, x + 11, y + 1, color);
      display.drawLine(x + 11, y + 1, x + 11, y + 6, color);
      display.drawLine(x + 11, y + 6, x + 13, y + 5, color);
      break;
  }
}

void drawWaveSelector(CardputerDisplay& display, Waveform option, Waveform selected, int x, const char* label) {
  const bool active = option == selected;
  const uint16_t fill = active ? CardputerDisplay::rgb565(25, 51, 34) : CardputerDisplay::rgb565(16, 24, 35);
  const uint16_t stroke = active ? colorGreen() : colorStroke();
  const uint16_t text = active ? CardputerDisplay::rgb565(215, 255, 225) : CardputerDisplay::rgb565(184, 200, 223);
  display.fillRoundRect(x, 19, 27, 13, 2, fill);
  display.drawRoundRect(x, 19, 27, 13, 2, stroke);
  display.drawText(label, x + 3, 22, text, 1);
  drawWaveIcon(display, option, x + 14, 22, text);
}

void drawSparkAxes(CardputerDisplay& display, int x, int y, int width, int height) {
  display.fillRect(x, y, width, height, colorPanel());
  display.drawRect(x, y, width, height, colorStroke());
  display.drawLine(x, y + height / 2, x + width - 1, y + height / 2, CardputerDisplay::rgb565(45, 57, 76));
}

void drawWavePreview(CardputerDisplay& display, const UiState& state) {
  constexpr int x = 10;
  constexpr int y = 41;
  constexpr int width = 87;
  constexpr int height = 37;

  drawSparkAxes(display, x, y, width, height);
  display.drawText("WAVE", x + 3, y + 4, colorMuted(), 1);
  display.drawText(waveformShortName(state.waveform), x + 55, y + 4, colorGreen(), 1);

  int prevX = x;
  int prevY = y + height / 2;
  for (int i = 0; i < 40; ++i) {
    const float phase = static_cast<float>(i) / 39.0f;
    const float sample = oscillatorSample(phase, state.waveform);
    const int px = x + 2 + (i * (width - 5)) / 39;
    const int py = y + height / 2 - static_cast<int>(sample * 13.0f);
    if (i > 0) display.drawLine(prevX, prevY, px, py, colorGreen());
    prevX = px;
    prevY = py;
  }
}

void drawOutputPreview(CardputerDisplay& display) {
  constexpr float INV_SQRT[MAX_POLYPHONY + 1] = {
      1.0f, 1.0f, 0.70710678f, 0.57735027f, 0.5f, 0.44721360f, 0.40824829f, 0.37796447f, 0.35355339f,
  };
  constexpr int x = 115;
  constexpr int y = 40;
  constexpr int width = 85;
  constexpr int height = 39;

  drawSparkAxes(display, x, y, width, height);
  display.drawText("OUT", x + 3, y + 4, colorMuted(), 1);

  SynthAudioState preview;
  copyAudioState(&preview);
  if (preview.activeCount == 0) return;

  ActiveNote notes[MAX_POLYPHONY] = {};
  for (int i = 0; i < MAX_POLYPHONY; ++i) notes[i] = preview.notes[i];

  int prevX = x;
  int prevY = y + height / 2;
  const float normalize = preview.activeCount <= MAX_POLYPHONY ? INV_SQRT[preview.activeCount] : INV_SQRT[MAX_POLYPHONY];
  for (int i = 0; i < 42; ++i) {
    float mixed = 0.0f;
    for (auto& note : notes) {
      if (!note.active) continue;

      mixed += oscillatorAudioSample(note.phase, preview.waveform) * PER_NOTE_GAIN * note.velocityGain;
      note.phase += note.phaseIncrement * 4.0f;
      if (note.phase >= 1.0f) note.phase -= floorf(note.phase);
    }

    mixed = clampFloat(mixed * normalize * preview.masterVolume, -1.0f, 1.0f);
    const int px = x + 2 + (i * (width - 5)) / 41;
    const int py = y + height / 2 - static_cast<int>(mixed * 15.0f);
    if (i > 0) display.drawLine(prevX, prevY, px, py, colorBlue());
    prevX = px;
    prevY = py;
  }
}

void drawVolume(CardputerDisplay& display, float volume) {
  constexpr int x = 220;
  constexpr int y = 85;
  constexpr int width = 10;
  constexpr int height = 34;

  display.drawRoundRect(x, y, width, height, 3, colorStroke());
  const int fillHeight = static_cast<int>((height - 4) * clampFloat(volume, 0.0f, 1.0f));
  if (fillHeight > 0) {
    display.fillRoundRect(x + 2, y + height - 2 - fillHeight, width - 4, fillHeight, 1, colorBlue());
  }
  display.drawText("VOL", 218, 121, colorMuted(), 1);
}

void drawWhiteKey(CardputerDisplay& display, int x, char label, uint8_t noteIndex, uint32_t pressedMask) {
  const bool active = (pressedMask & (1UL << noteIndex)) != 0;
  const uint16_t fill = active ? CardputerDisplay::rgb565(216, 236, 255) : CardputerDisplay::rgb565(248, 251, 255);
  const uint16_t stroke = active ? colorBlue() : CardputerDisplay::rgb565(15, 22, 31);
  display.fillRoundRect(x, 84, 10, 35, 2, fill);
  display.drawRoundRect(x, 84, 10, 35, 2, stroke);

  char text[2] = {label, '\0'};
  display.drawTextCentered(text, x + 5, 111, CardputerDisplay::rgb565(0, 0, 0), 1);
}

void drawBlackKey(CardputerDisplay& display, int x, char label, uint8_t noteIndex, uint32_t pressedMask) {
  const bool active = (pressedMask & (1UL << noteIndex)) != 0;
  const uint16_t fill = active ? CardputerDisplay::rgb565(37, 65, 95) : CardputerDisplay::rgb565(0, 0, 0);
  const uint16_t stroke = active ? colorBlue() : CardputerDisplay::rgb565(92, 92, 92);
  display.fillRoundRect(x, 84, 10, 21, 2, fill);
  display.drawRoundRect(x, 84, 10, 21, 2, stroke);

  char text[2] = {label, '\0'};
  display.drawTextCentered(text, x + 5, 93, CardputerDisplay::rgb565(248, 251, 255), 1);
}

void drawPiano(CardputerDisplay& display, uint32_t pressedMask) {
  struct WhiteKey {
    int x;
    char label;
    uint8_t noteIndex;
  };
  constexpr WhiteKey WHITE_KEYS[] = {
      {45, 'z', 0},   {55, 'x', 2},   {65, 'c', 4},   {75, 'v', 5},   {85, 'b', 7},
      {95, 'n', 9},   {105, 'm', 11}, {115, 'q', 12}, {125, 'w', 14}, {135, 'e', 16},
      {145, 'r', 17}, {155, 't', 19}, {165, 'y', 21}, {175, 'u', 23}, {185, 'i', 24},
  };

  struct BlackKey {
    int x;
    char label;
    uint8_t noteIndex;
  };
  constexpr BlackKey BLACK_KEYS[] = {
      {50, 's', 1},   {60, 'd', 3},   {80, 'g', 6},   {90, 'h', 8},   {100, 'j', 10},
      {120, '2', 13}, {130, '3', 15}, {150, '5', 18}, {160, '6', 20}, {170, '7', 22},
  };

  for (const auto& key : WHITE_KEYS) drawWhiteKey(display, key.x, key.label, key.noteIndex, pressedMask);
  for (const auto& key : BLACK_KEYS) drawBlackKey(display, key.x, key.label, key.noteIndex, pressedMask);

  display.drawTextCentered("C4", 52, 125, CardputerDisplay::rgb565(159, 178, 207), 1);
  display.drawTextCentered("C5", 122, 125, CardputerDisplay::rgb565(159, 178, 207), 1);
  display.drawTextCentered("C6", 192, 125, CardputerDisplay::rgb565(159, 178, 207), 1);
}

void drawUi(CardputerDisplay& display, const UiState& state) {
  display.clear(colorBg());
  display.drawText("pocketsynth", 9, 6, colorText(), 1);

  char polyText[8] = {};
  snprintf(polyText, sizeof(polyText), "%u/8", state.activeCount);
  display.drawText(polyText, 84, 6, CardputerDisplay::rgb565(159, 178, 207), 1);
  display.drawText("CHORD", 115, 6, colorMuted(), 1);
  display.drawText(state.chord, 149, 6, colorGreen(), 1);

  display.drawRoundRect(216, 6, 19, 7, 3, colorStroke());
  display.fillRoundRect(218, 8, 9, 3, 1, CardputerDisplay::rgb565(118, 187, 64));

  drawWaveSelector(display, Waveform::Sine, state.waveform, 8, "F1");
  drawWaveSelector(display, Waveform::Square, state.waveform, 38, "F2");
  drawWaveSelector(display, Waveform::Rectangle, state.waveform, 68, "F3");
  drawWaveSelector(display, Waveform::Saw, state.waveform, 98, "F4");
  drawWavePreview(display, state);
  drawOutputPreview(display);
  drawVolume(display, state.masterVolume);
  drawPiano(display, state.pressedMask);
}

}  // namespace

void uiTask(void*) {
  CardputerDisplay display;
  if (!display.begin()) {
    ESP_LOGE(TAG, "Display init failed");
    vTaskDelete(nullptr);
    return;
  }

  UiState state = {};
  uint32_t lastVersion = UINT32_MAX;
  for (;;) {
    copyUiState(&state);
    if (state.version != lastVersion) {
      drawUi(display, state);
      display.flush();
      lastVersion = state.version;
    }
    vTaskDelay(pdMS_TO_TICKS(UI_FRAME_MS));
  }
}

}  // namespace pocketsynth
