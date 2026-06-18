#include "led.h"

#include <algorithm>

CardputerLed::CardputerLed(CardputerRect bounds, bool on, uint16_t onFill, uint16_t offFill, uint16_t onStroke, uint16_t offStroke)
    : CardputerWidget(bounds), on_(on), onFill_(onFill), offFill_(offFill), onStroke_(onStroke), offStroke_(offStroke) {}

void CardputerLed::setOn(bool on) {
  if (on_ == on) return;
  on_ = on;
  invalidate();
}

void CardputerLed::draw(CardputerDisplay& display) {
  const int r = std::min(bounds_.w, bounds_.h) / 2;
  const int cx = bounds_.x + r;
  const int cy = bounds_.y + r;
  display.fillCircle(cx, cy, r, on_ ? onFill_ : offFill_);
  display.drawCircle(cx, cy, r, on_ ? onStroke_ : offStroke_);
}
