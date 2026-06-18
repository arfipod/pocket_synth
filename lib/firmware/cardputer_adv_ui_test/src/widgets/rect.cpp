#include "rect.h"

CardputerRectWidget::CardputerRectWidget(CardputerRect bounds, uint16_t fill, uint16_t stroke)
    : CardputerWidget(bounds), fill_(fill), stroke_(stroke) {}

void CardputerRectWidget::setFill(uint16_t fill) {
  if (fill_ == fill) return;
  fill_ = fill;
  invalidate();
}

void CardputerRectWidget::setStroke(uint16_t stroke) {
  if (stroke_ == stroke) return;
  stroke_ = stroke;
  invalidate();
}

void CardputerRectWidget::draw(CardputerDisplay& display) {
  display.fillRect(bounds_.x, bounds_.y, bounds_.w, bounds_.h, fill_);
  display.drawRect(bounds_.x, bounds_.y, bounds_.w, bounds_.h, stroke_);
}
