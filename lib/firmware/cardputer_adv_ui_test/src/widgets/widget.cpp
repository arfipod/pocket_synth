#include "widget.h"

CardputerWidget::CardputerWidget(CardputerRect bounds) : bounds_(bounds) {}

void CardputerWidget::setVisible(bool visible) {
  if (visible_ == visible) return;
  visible_ = visible;
  invalidate();
}
