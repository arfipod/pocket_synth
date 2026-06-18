#include "progress_bar.h"

#include <algorithm>
#include <cmath>

static float clampf(float value, float min, float max) {
  return std::max(min, std::min(max, value));
}

CardputerProgressBar::CardputerProgressBar(
    CardputerRect bounds,
    float value,
    float min,
    float max,
    uint16_t fill,
    uint16_t stroke,
    uint16_t background,
    int radius,
    CardputerProgressOrientation orientation)
    : CardputerWidget(bounds),
      value_(value),
      min_(min),
      max_(max),
      fill_(fill),
      stroke_(stroke),
      background_(background),
      radius_(radius),
      orientation_(orientation) {}

void CardputerProgressBar::setValue(float value) {
  value = clampf(value, min_, max_);
  if (std::fabs(value_ - value) < 0.5f) return;
  value_ = value;
  invalidate();
}

void CardputerProgressBar::setOrientation(CardputerProgressOrientation orientation) {
  if (orientation_ == orientation) return;
  orientation_ = orientation;
  invalidate();
}

float CardputerProgressBar::ratio() const {
  if (max_ == min_) return 0.0f;
  return clampf((value_ - min_) / (max_ - min_), 0.0f, 1.0f);
}

void CardputerProgressBar::draw(CardputerDisplay& display) {
  const int innerW = std::max(0, bounds_.w - 4);
  const int innerH = std::max(0, bounds_.h - 4);
  display.fillRoundRect(bounds_.x, bounds_.y, bounds_.w, bounds_.h, radius_, background_);
  display.drawRoundRect(bounds_.x, bounds_.y, bounds_.w, bounds_.h, radius_, stroke_);

  if (orientation_ == CardputerProgressOrientation::Vertical) {
    const int filled = std::max(0, static_cast<int>(innerH * ratio()));
    display.fillRoundRect(bounds_.x + 2, bounds_.y + 2 + innerH - filled, innerW, filled, std::max(0, radius_ - 2), fill_);
  } else {
    const int filled = std::max(0, static_cast<int>(innerW * ratio()));
    display.fillRoundRect(bounds_.x + 2, bounds_.y + 2, filled, innerH, std::max(0, radius_ - 2), fill_);
  }
}
