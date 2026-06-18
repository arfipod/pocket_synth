#include "text.h"

#include <string.h>

CardputerText::CardputerText(CardputerRect bounds, const char* text, uint16_t color, int scale, CardputerTextAlign align)
    : CardputerWidget(bounds), text_(text), color_(color), scale_(scale), align_(align) {}

void CardputerText::setText(const char* text) {
  if (text_ == text || (text_ && text && strcmp(text_, text) == 0)) return;
  text_ = text;
  invalidate();
}

void CardputerText::draw(CardputerDisplay& display) {
  const int cy = bounds_.y + bounds_.h / 2;
  if (align_ == CardputerTextAlign::Center) {
    display.drawTextCentered(text_, bounds_.x + bounds_.w / 2, cy, color_, scale_);
  } else if (align_ == CardputerTextAlign::Right) {
    display.drawText(text_, bounds_.x + bounds_.w - display.textWidth(text_, scale_), cy - (7 * scale_) / 2, color_, scale_);
  } else {
    display.drawText(text_, bounds_.x, cy - (7 * scale_) / 2, color_, scale_);
  }
}
