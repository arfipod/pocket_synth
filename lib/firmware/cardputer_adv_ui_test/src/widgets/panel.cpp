#include "panel.h"

CardputerPanel::CardputerPanel(CardputerRect bounds, uint16_t fill, uint16_t stroke, int radius)
    : CardputerWidget(bounds), fill_(fill), stroke_(stroke), radius_(radius) {}

void CardputerPanel::draw(CardputerDisplay& display) {
  display.fillRoundRect(bounds_.x, bounds_.y, bounds_.w, bounds_.h, radius_, fill_);
  display.drawRoundRect(bounds_.x, bounds_.y, bounds_.w, bounds_.h, radius_, stroke_);
}
