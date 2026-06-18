#pragma once

#include "widget.h"

class CardputerButton : public CardputerWidget {
 public:
  CardputerButton(CardputerRect bounds, const char* text, uint16_t fill, uint16_t stroke, uint16_t color, int radius, int scale);
  void setSelected(bool selected);
  void setActive(bool active);
  void draw(CardputerDisplay& display) override;

 private:
  const char* text_;
  uint16_t fill_;
  uint16_t stroke_;
  uint16_t color_;
  int radius_;
  int scale_;
  bool selected_ = false;
  bool active_ = false;
};
