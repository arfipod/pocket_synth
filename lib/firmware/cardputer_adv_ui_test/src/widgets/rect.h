#pragma once

#include "widget.h"

class CardputerRectWidget : public CardputerWidget {
 public:
  CardputerRectWidget(CardputerRect bounds, uint16_t fill, uint16_t stroke);
  void setFill(uint16_t fill);
  void setStroke(uint16_t stroke);
  void draw(CardputerDisplay& display) override;

 private:
  uint16_t fill_;
  uint16_t stroke_;
};
