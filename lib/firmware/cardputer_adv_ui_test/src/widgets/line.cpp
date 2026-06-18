#include "line.h"

#include <algorithm>
#include <cmath>

CardputerLine::CardputerLine(CardputerRect bounds, uint16_t color, int thickness)
    : CardputerWidget(bounds), color_(color), thickness_(std::max(1, thickness)) {}

void CardputerLine::setColor(uint16_t color) {
  if (color_ == color) return;
  color_ = color;
  invalidate();
}

void CardputerLine::setThickness(int thickness) {
  thickness = std::max(1, thickness);
  if (thickness_ == thickness) return;
  thickness_ = thickness;
  invalidate();
}

void CardputerLine::draw(CardputerDisplay& display) {
  const int x0 = bounds_.x;
  const int y0 = bounds_.y;
  const int x1 = bounds_.x + bounds_.w;
  const int y1 = bounds_.y + bounds_.h;
  const bool mostlyHorizontal = std::abs(bounds_.w) >= std::abs(bounds_.h);

  for (int offset = 0; offset < thickness_; ++offset) {
    const int delta = offset - thickness_ / 2;
    if (mostlyHorizontal) {
      display.drawLine(x0, y0 + delta, x1, y1 + delta, color_);
    } else {
      display.drawLine(x0 + delta, y0, x1 + delta, y1, color_);
    }
  }
}
