#pragma once

#include "widget.h"

class CardputerLine : public CardputerWidget {
 public:
  CardputerLine(CardputerRect bounds, uint16_t color, int thickness = 1);
  void setColor(uint16_t color);
  void setThickness(int thickness);
  void draw(CardputerDisplay& display) override;

 private:
  uint16_t color_;
  int thickness_;
};
