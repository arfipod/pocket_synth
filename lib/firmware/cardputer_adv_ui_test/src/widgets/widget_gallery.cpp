#include "widget_gallery.h"

#include "cardputer_ui_widgets.h"
#include <cmath>

static constexpr uint16_t c565(uint8_t r, uint8_t g, uint8_t b) {
  return static_cast<uint16_t>(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

static constexpr uint16_t COLOR_BG = c565(0x05, 0x07, 0x0b);
static constexpr uint16_t COLOR_PANEL = c565(0x11, 0x18, 0x27);
static constexpr uint16_t COLOR_STROKE = c565(0x39, 0x50, 0x6f);
static constexpr uint16_t COLOR_TEXT = c565(0xe7, 0xf0, 0xff);
static constexpr uint16_t COLOR_CYAN = c565(0x70, 0xd6, 0xff);
static constexpr uint16_t COLOR_GREEN = c565(0x9b, 0xff, 0xb7);
static constexpr uint16_t COLOR_AMBER = c565(0xf6, 0xc1, 0x77);
static constexpr uint16_t COLOR_PURPLE = c565(0xc0, 0x84, 0xfc);

static const uint16_t SAMPLE_IMAGE[8 * 8] = {
    COLOR_CYAN, COLOR_CYAN, COLOR_PANEL, COLOR_PANEL, COLOR_PANEL, COLOR_PANEL, COLOR_AMBER, COLOR_AMBER,
    COLOR_CYAN, COLOR_CYAN, COLOR_PANEL, COLOR_GREEN, COLOR_GREEN, COLOR_PANEL, COLOR_AMBER, COLOR_AMBER,
    COLOR_PANEL, COLOR_PANEL, COLOR_GREEN, COLOR_GREEN, COLOR_GREEN, COLOR_GREEN, COLOR_PANEL, COLOR_PANEL,
    COLOR_PANEL, COLOR_GREEN, COLOR_GREEN, COLOR_TEXT, COLOR_TEXT, COLOR_GREEN, COLOR_GREEN, COLOR_PANEL,
    COLOR_PANEL, COLOR_GREEN, COLOR_GREEN, COLOR_TEXT, COLOR_TEXT, COLOR_GREEN, COLOR_GREEN, COLOR_PANEL,
    COLOR_PANEL, COLOR_PANEL, COLOR_GREEN, COLOR_GREEN, COLOR_GREEN, COLOR_GREEN, COLOR_PANEL, COLOR_PANEL,
    COLOR_PURPLE, COLOR_PURPLE, COLOR_PANEL, COLOR_GREEN, COLOR_GREEN, COLOR_PANEL, COLOR_CYAN, COLOR_CYAN,
    COLOR_PURPLE, COLOR_PURPLE, COLOR_PANEL, COLOR_PANEL, COLOR_PANEL, COLOR_PANEL, COLOR_CYAN, COLOR_CYAN,
};

static float waveSample(uint32_t nowMs, int index) {
  const float t = nowMs / 1000.0f;
  const float x = index / 23.0f;
  return 50.0f +
         std::sin(t * 6.0f + x * 14.0f) * 26.0f +
         std::sin(t * 17.0f + x * 40.0f) * 12.0f;
}

void cardputer_draw_widget_gallery(CardputerDisplay& display, uint32_t nowMs) {
  display.clear(COLOR_BG);

  const float t = nowMs / 1000.0f;
  const float progressValue = 50.0f + std::sin(t * 1.9f) * 45.0f;
  const float gaugeValue = 50.0f + std::sin(t * 1.4f + 0.8f) * 50.0f;

  float samples[24] = {};
  for (int i = 0; i < 24; ++i) samples[i] = waveSample(nowMs, i);

  CardputerScreen screen;
  CardputerPanel panel({0, 0, 240, 135}, COLOR_PANEL, COLOR_STROKE, 6);
  CardputerText title({6, 4, 144, 14}, "WIDGET GALLERY", COLOR_TEXT, 1, CardputerTextAlign::Left);
  CardputerRectWidget rect({6, 22, 35, 20}, CardputerDisplay::rgb565(0x1d, 0x26, 0x35), COLOR_CYAN);
  CardputerLine line({47, 41, 42, -18}, COLOR_AMBER, 2);
  CardputerProgressBar progress({95, 22, 88, 12}, progressValue, 0, 100, COLOR_CYAN, COLOR_STROKE, CardputerDisplay::rgb565(0x0b, 0x10, 0x18), 4);
  CardputerProgressBar verticalProgress({224, 11, 10, 38}, progressValue, 0, 100, COLOR_PURPLE, COLOR_STROKE, CardputerDisplay::rgb565(0x0b, 0x10, 0x18), 4, CardputerProgressOrientation::Vertical);
  CardputerGauge gauge({185, 18, 34, 34}, gaugeValue, 0, 100, COLOR_AMBER, COLOR_STROKE, COLOR_PANEL);
  CardputerLed led({150, 5, 12, 12}, ((nowMs / 300) % 2) == 0, COLOR_GREEN, CardputerDisplay::rgb565(0x16, 0x3a, 0x25), COLOR_TEXT, COLOR_STROKE);
  CardputerIcon icon({166, 3, 16, 16}, CardputerIconKind::Wifi, COLOR_CYAN);
  CardputerSparkline sparkline({7, 54, 144, 31}, 0, 100, COLOR_GREEN, COLOR_STROKE, CardputerDisplay::rgb565(0x08, 0x0c, 0x14), 1, true);
  CardputerImage image({158, 58, 28, 28}, SAMPLE_IMAGE, 8, 8, COLOR_STROKE, CardputerDisplay::rgb565(0x0b, 0x10, 0x18));
  CardputerButton button({7, 96, 63, 28}, "Button", CardputerDisplay::rgb565(0x26, 0x31, 0x44), COLOR_CYAN, COLOR_TEXT, 5, 1);
  CardputerIcon sdIcon({78, 98, 24, 24}, CardputerIconKind::Sd, COLOR_TEXT);
  CardputerIcon batteryIcon({108, 98, 26, 24}, CardputerIconKind::Battery, COLOR_GREEN);
  CardputerIcon imageIcon({191, 58, 32, 28}, CardputerIconKind::Warning, COLOR_AMBER);

  sparkline.setSamples(samples, 24);
  button.setSelected(((nowMs / 1000) % 2) == 0);

  screen.add(&panel);
  screen.add(&title);
  screen.add(&rect);
  screen.add(&line);
  screen.add(&progress);
  screen.add(&verticalProgress);
  screen.add(&gauge);
  screen.add(&led);
  screen.add(&icon);
  screen.add(&sparkline);
  screen.add(&image);
  screen.add(&button);
  screen.add(&sdIcon);
  screen.add(&batteryIcon);
  screen.add(&imageIcon);
  screen.begin();
  screen.drawAll(display);
}
