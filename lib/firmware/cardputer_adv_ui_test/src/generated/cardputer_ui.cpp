#include "cardputer_ui.h"
#include "cardputer_ui_fonts.h"
#include "esp_timer.h"
#include <math.h>
#include <string.h>

static CardputerDisplay* ui_display = nullptr;

struct CardputerTransition {
  CardputerScreenId from;
  const char* element_id;
  CardputerUiEvent event;
  CardputerScreenId to;
};

static const CardputerTransition transitions[] = {
};

static void draw_pocketsynth_main_single_voice_compact_8_poly() {
  if (!ui_display) return;
  auto& display = *ui_display;
  display.clear(CardputerDisplay::rgb565(5, 7, 11));

  // App name (text) id=text-title
  display.drawText("pocketsynth", 9, 6, CardputerDisplay::rgb565(199, 215, 239), 1);

  // Polyphony status (text) id=text-polyphony
  display.drawText("0/8", 84, 6, CardputerDisplay::rgb565(159, 178, 207), 1);

  // Chord label (text) id=text-chord-label
  display.drawText("CHORD", 115, 6, CardputerDisplay::rgb565(143, 160, 187), 1);

  // Detected chord display (text) id=text-current-chord
  display.drawText("--", 149, 6, CardputerDisplay::rgb565(155, 255, 183), 1);

  // Battery level (progress) id=progress-battery
  display.drawRoundRect(216, 6, 19, 7, 3, CardputerDisplay::rgb565(52, 68, 93));
  display.fillRoundRect(218, 8, 9, 3, 1, CardputerDisplay::rgb565(118, 187, 64));

  // Volume label (text) id=text-volume-label
  display.drawText("VOL", 218, 121, CardputerDisplay::rgb565(143, 160, 187), 1);

  // Main voice volume (progress) id=progress-main-volume
  display.drawRoundRect(220, 85, 10, 34, 3, CardputerDisplay::rgb565(52, 68, 93));
  display.fillRoundRect(222, 96, 6, 21, 1, CardputerDisplay::rgb565(124, 199, 255));

  // Waveform SIN (Fn+1) (button) id=button-wave-sine
  display.fillRoundRect(8, 19, 27, 13, 2, CardputerDisplay::rgb565(25, 51, 34));
  display.drawRoundRect(8, 19, 27, 13, 2, CardputerDisplay::rgb565(155, 255, 183));
  display.drawTextCentered("F1", 22, 26, CardputerDisplay::rgb565(215, 255, 225), 1);

  // Waveform SQR (Fn+2) (button) id=button-wave-square
  display.fillRoundRect(38, 19, 27, 13, 2, CardputerDisplay::rgb565(16, 24, 35));
  display.drawRoundRect(38, 19, 27, 13, 2, CardputerDisplay::rgb565(52, 68, 93));
  display.drawTextCentered("F2", 52, 26, CardputerDisplay::rgb565(184, 200, 223), 1);

  // Waveform REC (Fn+3) (button) id=button-wave-rectangular
  display.fillRoundRect(68, 19, 27, 13, 2, CardputerDisplay::rgb565(16, 24, 35));
  display.drawRoundRect(68, 19, 27, 13, 2, CardputerDisplay::rgb565(52, 68, 93));
  display.drawTextCentered("F3", 82, 26, CardputerDisplay::rgb565(184, 200, 223), 1);

  // Waveform SAW (Fn+4) (button) id=button-wave-sawtooth
  display.fillRoundRect(98, 19, 27, 13, 2, CardputerDisplay::rgb565(16, 24, 35));
  display.drawRoundRect(98, 19, 27, 13, 2, CardputerDisplay::rgb565(52, 68, 93));
  display.drawTextCentered("F4", 112, 26, CardputerDisplay::rgb565(184, 200, 223), 1);

  // Sine waveform icon (sparkline) id=icon-wave-sine
  display.fillRect(20, 22, 12, 7, CardputerDisplay::rgb565(255, 255, 255));
  display.drawLine(20, 26, 22, 27, CardputerDisplay::rgb565(215, 255, 225));
  display.drawLine(22, 27, 24, 28, CardputerDisplay::rgb565(215, 255, 225));
  display.drawLine(24, 28, 26, 26, CardputerDisplay::rgb565(215, 255, 225));
  display.drawLine(26, 26, 28, 23, CardputerDisplay::rgb565(215, 255, 225));
  display.drawLine(28, 23, 30, 24, CardputerDisplay::rgb565(215, 255, 225));
  display.drawLine(30, 24, 32, 26, CardputerDisplay::rgb565(215, 255, 225));

  // Square waveform icon (sparkline) id=icon-wave-square
  display.fillRect(50, 22, 12, 7, CardputerDisplay::rgb565(255, 255, 255));
  display.drawLine(50, 24, 54, 24, CardputerDisplay::rgb565(215, 255, 225));
  display.drawLine(54, 24, 54, 27, CardputerDisplay::rgb565(215, 255, 225));
  display.drawLine(54, 27, 58, 27, CardputerDisplay::rgb565(215, 255, 225));
  display.drawLine(58, 27, 59, 24, CardputerDisplay::rgb565(215, 255, 225));
  display.drawLine(59, 24, 62, 24, CardputerDisplay::rgb565(215, 255, 225));

  // Rectangular pulse waveform icon (sparkline) id=icon-wave-rectangular
  display.fillRect(80, 22, 12, 7, CardputerDisplay::rgb565(255, 255, 255));
  display.drawLine(80, 24, 82, 24, CardputerDisplay::rgb565(215, 255, 225));
  display.drawLine(82, 24, 82, 27, CardputerDisplay::rgb565(215, 255, 225));
  display.drawLine(82, 27, 85, 27, CardputerDisplay::rgb565(215, 255, 225));
  display.drawLine(85, 27, 85, 24, CardputerDisplay::rgb565(215, 255, 225));
  display.drawLine(85, 24, 92, 24, CardputerDisplay::rgb565(215, 255, 225));

  // Sawtooth waveform icon (sparkline) id=icon-wave-sawtooth
  display.fillRect(110, 22, 12, 7, CardputerDisplay::rgb565(255, 255, 255));
  display.drawLine(110, 23, 120, 28, CardputerDisplay::rgb565(215, 255, 225));
  display.drawLine(120, 28, 120, 23, CardputerDisplay::rgb565(215, 255, 225));
  display.drawLine(120, 23, 122, 24, CardputerDisplay::rgb565(215, 255, 225));

  // Selected waveform preview (sparkline) id=sparkline-wave-preview
  display.fillRect(10, 41, 87, 37, CardputerDisplay::rgb565(11, 16, 24));
  display.drawRect(10, 41, 87, 37, CardputerDisplay::rgb565(82, 97, 121));
  display.drawLine(10, 60, 96, 60, CardputerDisplay::rgb565(82, 97, 121));
  display.drawLine(54, 41, 54, 77, CardputerDisplay::rgb565(82, 97, 121));
  const int spark_samples_sparkline_wave_preview = 22;
  const float spark_t_sparkline_wave_preview = esp_timer_get_time() / 1000000.0f;
  int spark_prev_x_sparkline_wave_preview = 10;
  int spark_prev_y_sparkline_wave_preview = 60;
  for (int i = 0; i < spark_samples_sparkline_wave_preview; ++i) {
    const float x_ratio = spark_samples_sparkline_wave_preview <= 1 ? 0.0f : (float)i / (float)(spark_samples_sparkline_wave_preview - 1);
    const float sample = fminf(100.0f, fmaxf(0.0f, 50.0f + sinf(spark_t_sparkline_wave_preview * 7.0f + x_ratio * 24.0f) * (22.0f + 18.0f * sinf(spark_t_sparkline_wave_preview * 2.1f)) + sinf(spark_t_sparkline_wave_preview * 18.0f + x_ratio * 53.0f) * 16.0f));
    const int x = 10 + (int)(x_ratio * 86);
    const int y = 77 - (int)((sample / 100.0f) * 36);
    if (i > 0) display.drawLine(spark_prev_x_sparkline_wave_preview, spark_prev_y_sparkline_wave_preview, x, y, CardputerDisplay::rgb565(155, 255, 183));
    spark_prev_x_sparkline_wave_preview = x;
    spark_prev_y_sparkline_wave_preview = y;
  }

  // Polyphonic output preview (sparkline) id=sparkline-output-preview
  display.fillRect(115, 40, 85, 39, CardputerDisplay::rgb565(11, 16, 24));
  display.drawRect(115, 40, 85, 39, CardputerDisplay::rgb565(82, 97, 121));
  display.drawLine(115, 60, 199, 60, CardputerDisplay::rgb565(82, 97, 121));
  display.drawLine(158, 40, 158, 78, CardputerDisplay::rgb565(82, 97, 121));
  const int spark_samples_sparkline_output_preview = 21;
  const float spark_t_sparkline_output_preview = esp_timer_get_time() / 1000000.0f;
  int spark_prev_x_sparkline_output_preview = 115;
  int spark_prev_y_sparkline_output_preview = 60;
  for (int i = 0; i < spark_samples_sparkline_output_preview; ++i) {
    const float x_ratio = spark_samples_sparkline_output_preview <= 1 ? 0.0f : (float)i / (float)(spark_samples_sparkline_output_preview - 1);
    const float sample = fminf(100.0f, fmaxf(0.0f, 50.0f + sinf(spark_t_sparkline_output_preview * 7.0f + x_ratio * 24.0f) * (22.0f + 18.0f * sinf(spark_t_sparkline_output_preview * 2.1f)) + sinf(spark_t_sparkline_output_preview * 18.0f + x_ratio * 53.0f) * 16.0f));
    const int x = 115 + (int)(x_ratio * 84);
    const int y = 78 - (int)((sample / 100.0f) * 38);
    if (i > 0) display.drawLine(spark_prev_x_sparkline_output_preview, spark_prev_y_sparkline_output_preview, x, y, CardputerDisplay::rgb565(124, 199, 255));
    spark_prev_x_sparkline_output_preview = x;
    spark_prev_y_sparkline_output_preview = y;
  }

  // Selected waveform preview label (text) id=text-wave-preview-label
  display.drawText("WAVE", 13, 53, CardputerDisplay::rgb565(143, 160, 187), 1);

  // Output preview label (text) id=text-output-preview-label
  display.drawText("OUT", 118, 51, CardputerDisplay::rgb565(143, 160, 187), 1);

  // Piano key C4 (z) (button) id=button-piano-white-z
  display.fillRoundRect(45, 84, 10, 35, 2, CardputerDisplay::rgb565(255, 255, 255));
  display.drawRoundRect(45, 84, 10, 35, 2, CardputerDisplay::rgb565(0, 0, 0));
  display.drawTextCentered("", 50, 102, CardputerDisplay::rgb565(0, 0, 0), 1);

  // Piano key D4 (x) (button) id=button-piano-white-x
  display.fillRoundRect(55, 84, 10, 35, 2, CardputerDisplay::rgb565(255, 255, 255));
  display.drawRoundRect(55, 84, 10, 35, 2, CardputerDisplay::rgb565(0, 0, 0));
  display.drawTextCentered("", 60, 102, CardputerDisplay::rgb565(0, 0, 0), 1);

  // Piano key E4 (c) (button) id=button-piano-white-c
  display.fillRoundRect(65, 84, 10, 35, 2, CardputerDisplay::rgb565(255, 255, 255));
  display.drawRoundRect(65, 84, 10, 35, 2, CardputerDisplay::rgb565(0, 0, 0));
  display.drawTextCentered("", 70, 102, CardputerDisplay::rgb565(0, 0, 0), 1);

  // Piano key F4 (v) (button) id=button-piano-white-v
  display.fillRoundRect(75, 84, 10, 35, 2, CardputerDisplay::rgb565(255, 255, 255));
  display.drawRoundRect(75, 84, 10, 35, 2, CardputerDisplay::rgb565(0, 0, 0));
  display.drawTextCentered("", 80, 102, CardputerDisplay::rgb565(0, 0, 0), 1);

  // Piano key G4 (b) (button) id=button-piano-white-b
  display.fillRoundRect(85, 84, 10, 35, 2, CardputerDisplay::rgb565(255, 255, 255));
  display.drawRoundRect(85, 84, 10, 35, 2, CardputerDisplay::rgb565(0, 0, 0));
  display.drawTextCentered("", 90, 102, CardputerDisplay::rgb565(0, 0, 0), 1);

  // Piano key A4 (n) (button) id=button-piano-white-n
  display.fillRoundRect(95, 84, 10, 35, 2, CardputerDisplay::rgb565(255, 255, 255));
  display.drawRoundRect(95, 84, 10, 35, 2, CardputerDisplay::rgb565(0, 0, 0));
  display.drawTextCentered("", 100, 102, CardputerDisplay::rgb565(0, 0, 0), 1);

  // Piano key B4 (m) (button) id=button-piano-white-m
  display.fillRoundRect(105, 84, 10, 35, 2, CardputerDisplay::rgb565(255, 255, 255));
  display.drawRoundRect(105, 84, 10, 35, 2, CardputerDisplay::rgb565(0, 0, 0));
  display.drawTextCentered("", 110, 102, CardputerDisplay::rgb565(0, 0, 0), 1);

  // Piano key C5 (q) (button) id=button-piano-white-q
  display.fillRoundRect(115, 84, 10, 35, 2, CardputerDisplay::rgb565(255, 255, 255));
  display.drawRoundRect(115, 84, 10, 35, 2, CardputerDisplay::rgb565(0, 0, 0));
  display.drawTextCentered("", 120, 102, CardputerDisplay::rgb565(0, 0, 0), 1);

  // Piano key D5 (w) (button) id=button-piano-white-w
  display.fillRoundRect(125, 84, 10, 35, 2, CardputerDisplay::rgb565(255, 255, 255));
  display.drawRoundRect(125, 84, 10, 35, 2, CardputerDisplay::rgb565(0, 0, 0));
  display.drawTextCentered("", 130, 102, CardputerDisplay::rgb565(0, 0, 0), 1);

  // Piano key E5 (e) (button) id=button-piano-white-e
  display.fillRoundRect(135, 84, 10, 35, 2, CardputerDisplay::rgb565(255, 255, 255));
  display.drawRoundRect(135, 84, 10, 35, 2, CardputerDisplay::rgb565(0, 0, 0));
  display.drawTextCentered("", 140, 102, CardputerDisplay::rgb565(0, 0, 0), 1);

  // Piano key F5 (r) (button) id=button-piano-white-r
  display.fillRoundRect(145, 84, 10, 35, 2, CardputerDisplay::rgb565(255, 255, 255));
  display.drawRoundRect(145, 84, 10, 35, 2, CardputerDisplay::rgb565(0, 0, 0));
  display.drawTextCentered("", 150, 102, CardputerDisplay::rgb565(0, 0, 0), 1);

  // Piano key G5 (t) (button) id=button-piano-white-t
  display.fillRoundRect(155, 84, 10, 35, 2, CardputerDisplay::rgb565(255, 255, 255));
  display.drawRoundRect(155, 84, 10, 35, 2, CardputerDisplay::rgb565(0, 0, 0));
  display.drawTextCentered("", 160, 102, CardputerDisplay::rgb565(0, 0, 0), 1);

  // Piano key A5 (y) (button) id=button-piano-white-y
  display.fillRoundRect(165, 84, 10, 35, 2, CardputerDisplay::rgb565(255, 255, 255));
  display.drawRoundRect(165, 84, 10, 35, 2, CardputerDisplay::rgb565(0, 0, 0));
  display.drawTextCentered("", 170, 102, CardputerDisplay::rgb565(0, 0, 0), 1);

  // Piano key B5 (u) (button) id=button-piano-white-u
  display.fillRoundRect(175, 84, 10, 35, 2, CardputerDisplay::rgb565(255, 255, 255));
  display.drawRoundRect(175, 84, 10, 35, 2, CardputerDisplay::rgb565(0, 0, 0));
  display.drawTextCentered("", 180, 102, CardputerDisplay::rgb565(0, 0, 0), 1);

  // Piano key C6 (i) (button) id=button-piano-white-i
  display.fillRoundRect(185, 84, 10, 35, 2, CardputerDisplay::rgb565(255, 255, 255));
  display.drawRoundRect(185, 84, 10, 35, 2, CardputerDisplay::rgb565(0, 0, 0));
  display.drawTextCentered("", 190, 102, CardputerDisplay::rgb565(0, 0, 0), 1);

  // Piano key C#4/Db4 (s) (button) id=button-piano-black-s
  display.fillRoundRect(50, 84, 10, 21, 2, CardputerDisplay::rgb565(0, 0, 0));
  display.drawRoundRect(50, 84, 10, 21, 2, CardputerDisplay::rgb565(92, 92, 92));
  display.drawTextCentered("", 55, 95, CardputerDisplay::rgb565(248, 251, 255), 1);

  // Piano key D#4/Eb4 (d) (button) id=button-piano-black-d
  display.fillRoundRect(60, 84, 10, 21, 2, CardputerDisplay::rgb565(0, 0, 0));
  display.drawRoundRect(60, 84, 10, 21, 2, CardputerDisplay::rgb565(92, 92, 92));
  display.drawTextCentered("", 65, 95, CardputerDisplay::rgb565(248, 251, 255), 1);

  // Piano key F#4/Gb4 (g) (button) id=button-piano-black-g
  display.fillRoundRect(80, 84, 10, 21, 2, CardputerDisplay::rgb565(0, 0, 0));
  display.drawRoundRect(80, 84, 10, 21, 2, CardputerDisplay::rgb565(92, 92, 92));
  display.drawTextCentered("", 85, 95, CardputerDisplay::rgb565(248, 251, 255), 1);

  // Piano key G#4/Ab4 (h) (button) id=button-piano-black-h
  display.fillRoundRect(90, 84, 10, 21, 2, CardputerDisplay::rgb565(0, 0, 0));
  display.drawRoundRect(90, 84, 10, 21, 2, CardputerDisplay::rgb565(92, 92, 92));
  display.drawTextCentered("", 95, 95, CardputerDisplay::rgb565(248, 251, 255), 1);

  // Piano key A#4/Bb4 (j) (button) id=button-piano-black-j
  display.fillRoundRect(100, 84, 10, 21, 2, CardputerDisplay::rgb565(0, 0, 0));
  display.drawRoundRect(100, 84, 10, 21, 2, CardputerDisplay::rgb565(92, 92, 92));
  display.drawTextCentered("", 105, 95, CardputerDisplay::rgb565(248, 251, 255), 1);

  // Piano key C#5/Db5 (2) (button) id=button-piano-black-2
  display.fillRoundRect(120, 84, 10, 21, 2, CardputerDisplay::rgb565(0, 0, 0));
  display.drawRoundRect(120, 84, 10, 21, 2, CardputerDisplay::rgb565(92, 92, 92));
  display.drawTextCentered("", 125, 95, CardputerDisplay::rgb565(248, 251, 255), 1);

  // Piano key D#5/Eb5 (3) (button) id=button-piano-black-3
  display.fillRoundRect(130, 84, 10, 21, 2, CardputerDisplay::rgb565(0, 0, 0));
  display.drawRoundRect(130, 84, 10, 21, 2, CardputerDisplay::rgb565(92, 92, 92));
  display.drawTextCentered("", 135, 95, CardputerDisplay::rgb565(248, 251, 255), 1);

  // Piano key F#5/Gb5 (5) (button) id=button-piano-black-5
  display.fillRoundRect(150, 84, 10, 21, 2, CardputerDisplay::rgb565(0, 0, 0));
  display.drawRoundRect(150, 84, 10, 21, 2, CardputerDisplay::rgb565(92, 92, 92));
  display.drawTextCentered("", 155, 95, CardputerDisplay::rgb565(248, 251, 255), 1);

  // Piano key G#5/Ab5 (6) (button) id=button-piano-black-6
  display.fillRoundRect(160, 84, 10, 21, 2, CardputerDisplay::rgb565(0, 0, 0));
  display.drawRoundRect(160, 84, 10, 21, 2, CardputerDisplay::rgb565(92, 92, 92));
  display.drawTextCentered("", 165, 95, CardputerDisplay::rgb565(248, 251, 255), 1);

  // Piano key A#5/Bb5 (7) (button) id=button-piano-black-7
  display.fillRoundRect(170, 84, 10, 21, 2, CardputerDisplay::rgb565(0, 0, 0));
  display.drawRoundRect(170, 84, 10, 21, 2, CardputerDisplay::rgb565(92, 92, 92));
  display.drawTextCentered("", 175, 95, CardputerDisplay::rgb565(248, 251, 255), 1);

  // Label C4 (z) (text) id=text-label-white-z
  display.drawTextCentered("z", 50, 111, CardputerDisplay::rgb565(0, 0, 0), 1);

  // Label D4 (x) (text) id=text-label-white-x
  display.drawTextCentered("x", 60, 111, CardputerDisplay::rgb565(0, 0, 0), 1);

  // Label E4 (c) (text) id=text-label-white-c
  display.drawTextCentered("c", 70, 111, CardputerDisplay::rgb565(0, 0, 0), 1);

  // Label F4 (v) (text) id=text-label-white-v
  display.drawTextCentered("v", 80, 111, CardputerDisplay::rgb565(0, 0, 0), 1);

  // Label G4 (b) (text) id=text-label-white-b
  display.drawTextCentered("b", 90, 111, CardputerDisplay::rgb565(0, 0, 0), 1);

  // Label A4 (n) (text) id=text-label-white-n
  display.drawTextCentered("n", 100, 111, CardputerDisplay::rgb565(0, 0, 0), 1);

  // Label B4 (m) (text) id=text-label-white-m
  display.drawTextCentered("m", 110, 111, CardputerDisplay::rgb565(0, 0, 0), 1);

  // Label C5 (q) (text) id=text-label-white-q
  display.drawTextCentered("q", 120, 111, CardputerDisplay::rgb565(0, 0, 0), 1);

  // Label D5 (w) (text) id=text-label-white-w
  display.drawTextCentered("w", 130, 111, CardputerDisplay::rgb565(0, 0, 0), 1);

  // Label E5 (e) (text) id=text-label-white-e
  display.drawTextCentered("e", 140, 111, CardputerDisplay::rgb565(0, 0, 0), 1);

  // Label F5 (r) (text) id=text-label-white-r
  display.drawTextCentered("r", 150, 111, CardputerDisplay::rgb565(0, 0, 0), 1);

  // Label G5 (t) (text) id=text-label-white-t
  display.drawTextCentered("t", 160, 111, CardputerDisplay::rgb565(0, 0, 0), 1);

  // Label A5 (y) (text) id=text-label-white-y
  display.drawTextCentered("y", 170, 111, CardputerDisplay::rgb565(0, 0, 0), 1);

  // Label B5 (u) (text) id=text-label-white-u
  display.drawTextCentered("u", 180, 111, CardputerDisplay::rgb565(0, 0, 0), 1);

  // Label C6 (i) (text) id=text-label-white-i
  display.drawTextCentered("i", 190, 111, CardputerDisplay::rgb565(0, 0, 0), 1);

  // Label C#4/Db4 (s) (text) id=text-label-black-s
  display.drawTextCentered("s", 55, 93, CardputerDisplay::rgb565(248, 251, 255), 1);

  // Label D#4/Eb4 (d) (text) id=text-label-black-d
  display.drawTextCentered("d", 65, 93, CardputerDisplay::rgb565(248, 251, 255), 1);

  // Label F#4/Gb4 (g) (text) id=text-label-black-g
  display.drawTextCentered("g", 85, 93, CardputerDisplay::rgb565(248, 251, 255), 1);

  // Label G#4/Ab4 (h) (text) id=text-label-black-h
  display.drawTextCentered("h", 95, 93, CardputerDisplay::rgb565(248, 251, 255), 1);

  // Label A#4/Bb4 (j) (text) id=text-label-black-j
  display.drawTextCentered("j", 105, 93, CardputerDisplay::rgb565(248, 251, 255), 1);

  // Label C#5/Db5 (2) (text) id=text-label-black-2
  display.drawTextCentered("2", 125, 93, CardputerDisplay::rgb565(248, 251, 255), 1);

  // Label D#5/Eb5 (3) (text) id=text-label-black-3
  display.drawTextCentered("3", 135, 93, CardputerDisplay::rgb565(248, 251, 255), 1);

  // Label F#5/Gb5 (5) (text) id=text-label-black-5
  display.drawTextCentered("5", 155, 93, CardputerDisplay::rgb565(248, 251, 255), 1);

  // Label G#5/Ab5 (6) (text) id=text-label-black-6
  display.drawTextCentered("6", 165, 93, CardputerDisplay::rgb565(248, 251, 255), 1);

  // Label A#5/Bb5 (7) (text) id=text-label-black-7
  display.drawTextCentered("7", 175, 93, CardputerDisplay::rgb565(248, 251, 255), 1);

  // Octave marker C4 (text) id=text-note-marker-c4
  display.drawTextCentered("C4", 52, 125, CardputerDisplay::rgb565(159, 178, 207), 1);

  // Octave marker C5 (text) id=text-note-marker-c5
  display.drawTextCentered("C5", 122, 125, CardputerDisplay::rgb565(159, 178, 207), 1);

  // Octave marker C6 (text) id=text-note-marker-c6
  display.drawTextCentered("C6", 192, 125, CardputerDisplay::rgb565(159, 178, 207), 1);

}

void cardputer_ui_init(CardputerDisplay* display) {
  ui_display = display;
}

void cardputer_ui_draw(CardputerScreenId screen) {
  switch (screen) {
    case CARDPUTER_SCREEN_POCKETSYNTH_MAIN_SINGLE_VOICE_COMPACT_8_POLY: draw_pocketsynth_main_single_voice_compact_8_poly(); break;
  }
}

CardputerScreenId cardputer_ui_handle_event(CardputerScreenId current, CardputerUiEvent event) {
  return cardputer_ui_handle_element_event(current, nullptr, event);
}

CardputerScreenId cardputer_ui_handle_element_event(CardputerScreenId current, const char* elementId, CardputerUiEvent event) {
  for (const auto& transition : transitions) {
    const bool element_matches = elementId == nullptr || transition.element_id == nullptr || strcmp(transition.element_id, elementId) == 0;
    if (transition.from == current && transition.event == event && element_matches) return transition.to;
  }
  return current;
}
