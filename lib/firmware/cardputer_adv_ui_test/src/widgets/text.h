#pragma once

#include "widget.h"

class CardputerText : public CardputerWidget {
 public:
  CardputerText(CardputerRect bounds, const char* text, uint16_t color, int scale, CardputerTextAlign align);
  void setText(const char* text);
  void draw(CardputerDisplay& display) override;

 private:
  const char* text_;
  uint16_t color_;
  int scale_;
  CardputerTextAlign align_;
};
