#pragma once

#include "widget.h"

class CardputerPanel : public CardputerWidget {
 public:
  CardputerPanel(CardputerRect bounds, uint16_t fill, uint16_t stroke, int radius);
  void draw(CardputerDisplay& display) override;

 private:
  uint16_t fill_;
  uint16_t stroke_;
  int radius_;
};
