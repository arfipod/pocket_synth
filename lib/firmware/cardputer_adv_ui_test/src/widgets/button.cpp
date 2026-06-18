#include "button.h"

CardputerButton::CardputerButton(CardputerRect bounds, const char* text, uint16_t fill, uint16_t stroke, uint16_t color, int radius, int scale)
    : CardputerWidget({bounds.x - 2, bounds.y - 2, bounds.w + 4, bounds.h + 4}),
      text_(text),
      fill_(fill),
      stroke_(stroke),
      color_(color),
      radius_(radius),
      scale_(scale) {}

void CardputerButton::setSelected(bool selected) {
  if (selected_ == selected) return;
  selected_ = selected;
  invalidate();
}

void CardputerButton::setActive(bool active) {
  if (active_ == active) return;
  active_ = active;
  invalidate();
}

void CardputerButton::draw(CardputerDisplay& display) {
  const int x = bounds_.x + 2;
  const int y = bounds_.y + 2;
  const int w = bounds_.w - 4;
  const int h = bounds_.h - 4;

  display.fillRoundRect(x, y, w, h, radius_, fill_);
  display.drawRoundRect(x, y, w, h, radius_, stroke_);
  display.drawTextCentered(text_, x + w / 2, y + h / 2, color_, scale_);

  if (selected_) {
    const uint16_t accent = active_
        ? CardputerDisplay::rgb565(0x4a, 0xde, 0x80)
        : CardputerDisplay::rgb565(0xf6, 0xc1, 0x77);
    display.drawRoundRect(bounds_.x, bounds_.y, bounds_.w, bounds_.h, radius_ + 2, accent);
    display.drawRoundRect(bounds_.x + 1, bounds_.y + 1, bounds_.w - 2, bounds_.h - 2, radius_ + 1, accent);
  }
}
