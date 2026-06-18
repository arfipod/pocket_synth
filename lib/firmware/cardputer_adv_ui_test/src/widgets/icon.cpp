#include "icon.h"

#include <algorithm>

CardputerIcon::CardputerIcon(CardputerRect bounds, CardputerIconKind kind, uint16_t color)
    : CardputerWidget(bounds), kind_(kind), color_(color) {}

void CardputerIcon::setKind(CardputerIconKind kind) {
  if (kind_ == kind) return;
  kind_ = kind;
  invalidate();
}

void CardputerIcon::setColor(uint16_t color) {
  if (color_ == color) return;
  color_ = color;
  invalidate();
}

void CardputerIcon::draw(CardputerDisplay& display) {
  const int cx = bounds_.x + bounds_.w / 2;
  const int cy = bounds_.y + bounds_.h / 2;
  const int size = std::min(bounds_.w, bounds_.h);

  switch (kind_) {
    case CardputerIconKind::Wifi:
      drawWifi(display, cx, cy, size);
      break;
    case CardputerIconKind::Sd:
      drawSd(display);
      break;
    case CardputerIconKind::Battery:
      drawBattery(display);
      break;
    case CardputerIconKind::Audio:
      drawAudio(display, cx, cy, size);
      break;
    case CardputerIconKind::Imu:
      drawImu(display);
      break;
    case CardputerIconKind::Play:
      drawPlay(display);
      break;
    case CardputerIconKind::Pause:
      drawPause(display);
      break;
    case CardputerIconKind::Warning:
      drawWarning(display);
      break;
  }
}

void CardputerIcon::drawWifi(CardputerDisplay& display, int cx, int cy, int size) const {
  const int r = size / 2;
  display.drawLine(cx - r, cy, cx, cy - r / 2, color_);
  display.drawLine(cx, cy - r / 2, cx + r, cy, color_);
  display.drawLine(cx - r / 2, cy + r / 4, cx, cy, color_);
  display.drawLine(cx, cy, cx + r / 2, cy + r / 4, color_);
  display.fillCircle(cx, cy + r / 2, std::max(1, size / 10), color_);
}

void CardputerIcon::drawSd(CardputerDisplay& display) const {
  const int notch = std::max(2, bounds_.w / 5);
  display.drawRect(bounds_.x + 2, bounds_.y + 1, bounds_.w - 4, bounds_.h - 2, color_);
  display.drawLine(bounds_.x + bounds_.w - notch - 2, bounds_.y + 1, bounds_.x + bounds_.w - 2, bounds_.y + notch + 1, color_);
  display.drawText("SD", bounds_.x + 4, bounds_.y + bounds_.h / 2 - 4, color_, 1);
}

void CardputerIcon::drawBattery(CardputerDisplay& display) const {
  const int capW = std::max(2, bounds_.w / 8);
  display.drawRect(bounds_.x, bounds_.y + bounds_.h / 4, bounds_.w - capW, bounds_.h / 2, color_);
  display.fillRect(bounds_.x + bounds_.w - capW, bounds_.y + bounds_.h / 3, capW, bounds_.h / 3, color_);
  display.fillRect(bounds_.x + 3, bounds_.y + bounds_.h / 4 + 3, std::max(1, bounds_.w - capW - 6), std::max(1, bounds_.h / 2 - 6), color_);
}

void CardputerIcon::drawAudio(CardputerDisplay& display, int cx, int cy, int size) const {
  display.fillRect(bounds_.x + 2, cy - size / 6, size / 5, size / 3, color_);
  display.drawLine(bounds_.x + size / 5 + 2, cy - size / 6, cx, cy - size / 3, color_);
  display.drawLine(cx, cy - size / 3, cx, cy + size / 3, color_);
  display.drawLine(cx, cy + size / 3, bounds_.x + size / 5 + 2, cy + size / 6, color_);
  display.drawCircle(cx + size / 5, cy, size / 4, color_);
}

void CardputerIcon::drawImu(CardputerDisplay& display) const {
  display.drawRect(bounds_.x + 3, bounds_.y + 3, bounds_.w - 6, bounds_.h - 6, color_);
  display.drawLine(bounds_.x + bounds_.w / 2, bounds_.y + 1, bounds_.x + bounds_.w / 2, bounds_.y + bounds_.h - 2, color_);
  display.drawLine(bounds_.x + 1, bounds_.y + bounds_.h / 2, bounds_.x + bounds_.w - 2, bounds_.y + bounds_.h / 2, color_);
}

void CardputerIcon::drawPlay(CardputerDisplay& display) const {
  const int x0 = bounds_.x + bounds_.w / 4;
  const int y0 = bounds_.y + bounds_.h / 5;
  const int x1 = bounds_.x + bounds_.w * 3 / 4;
  const int cy = bounds_.y + bounds_.h / 2;
  for (int y = y0; y <= bounds_.y + bounds_.h * 4 / 5; ++y) {
    const int half = std::abs(y - cy);
    display.drawLine(x0, y, x1 - half, cy, color_);
  }
}

void CardputerIcon::drawPause(CardputerDisplay& display) const {
  display.fillRect(bounds_.x + bounds_.w / 4, bounds_.y + bounds_.h / 5, bounds_.w / 6, bounds_.h * 3 / 5, color_);
  display.fillRect(bounds_.x + bounds_.w * 7 / 12, bounds_.y + bounds_.h / 5, bounds_.w / 6, bounds_.h * 3 / 5, color_);
}

void CardputerIcon::drawWarning(CardputerDisplay& display) const {
  const int cx = bounds_.x + bounds_.w / 2;
  const int top = bounds_.y + 2;
  const int bottom = bounds_.y + bounds_.h - 2;
  display.drawLine(cx, top, bounds_.x + 2, bottom, color_);
  display.drawLine(cx, top, bounds_.x + bounds_.w - 2, bottom, color_);
  display.drawLine(bounds_.x + 2, bottom, bounds_.x + bounds_.w - 2, bottom, color_);
  display.drawLine(cx, bounds_.y + bounds_.h / 3, cx, bounds_.y + bounds_.h * 2 / 3, color_);
  display.drawPixel(cx, bounds_.y + bounds_.h * 3 / 4, color_);
}
