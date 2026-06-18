#include "gauge.h"

#include <algorithm>
#include <cmath>

static float clampf(float value, float min, float max) {
  return std::max(min, std::min(max, value));
}

CardputerGauge::CardputerGauge(CardputerRect bounds, float value, float min, float max, uint16_t fill, uint16_t stroke, uint16_t background)
    : CardputerWidget(bounds), value_(value), min_(min), max_(max), fill_(fill), stroke_(stroke), background_(background) {}

void CardputerGauge::setValue(float value) {
  value = clampf(value, min_, max_);
  if (std::fabs(value_ - value) < 0.5f) return;
  value_ = value;
  invalidate();
}

float CardputerGauge::ratio() const {
  if (max_ == min_) return 0.0f;
  return clampf((value_ - min_) / (max_ - min_), 0.0f, 1.0f);
}

void CardputerGauge::draw(CardputerDisplay& display) {
  const int cx = bounds_.x + bounds_.w / 2;
  const int cy = bounds_.y + bounds_.h / 2;
  const int radius = std::min(bounds_.w, bounds_.h) / 2 - 2;
  const float angle = (-140.0f + ratio() * 280.0f) * 3.14159265f / 180.0f;
  const int nx = cx + static_cast<int>(std::cos(angle) * (radius - 7));
  const int ny = cy + static_cast<int>(std::sin(angle) * (radius - 7));

  display.fillRect(bounds_.x - 1, bounds_.y - 1, bounds_.w + 2, bounds_.h + 2, background_);
  display.drawCircle(cx, cy, radius, stroke_);
  display.drawCircle(cx, cy, radius - 6, stroke_);
  display.drawLine(cx, cy, nx, ny, fill_);
  display.fillCircle(cx, cy, 2, fill_);
}
