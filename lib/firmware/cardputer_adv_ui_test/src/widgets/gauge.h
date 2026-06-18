#pragma once

#include "widget.h"

class CardputerGauge : public CardputerWidget {
 public:
  CardputerGauge(CardputerRect bounds, float value, float min, float max, uint16_t fill, uint16_t stroke, uint16_t background);
  void setValue(float value);
  void draw(CardputerDisplay& display) override;

 private:
  float ratio() const;

  float value_;
  float min_;
  float max_;
  uint16_t fill_;
  uint16_t stroke_;
  uint16_t background_;
};
