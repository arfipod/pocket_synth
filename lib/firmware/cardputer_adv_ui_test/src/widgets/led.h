#pragma once

#include "widget.h"

class CardputerLed : public CardputerWidget {
 public:
  CardputerLed(CardputerRect bounds, bool on, uint16_t onFill, uint16_t offFill, uint16_t onStroke, uint16_t offStroke);
  void setOn(bool on);
  void draw(CardputerDisplay& display) override;

 private:
  bool on_;
  uint16_t onFill_;
  uint16_t offFill_;
  uint16_t onStroke_;
  uint16_t offStroke_;
};
