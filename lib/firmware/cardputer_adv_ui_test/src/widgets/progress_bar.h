#pragma once

#include "widget.h"

enum class CardputerProgressOrientation {
  Horizontal,
  Vertical,
};

class CardputerProgressBar : public CardputerWidget {
 public:
  CardputerProgressBar(
      CardputerRect bounds,
      float value,
      float min,
      float max,
      uint16_t fill,
      uint16_t stroke,
      uint16_t background,
      int radius,
      CardputerProgressOrientation orientation = CardputerProgressOrientation::Horizontal);
  void setValue(float value);
  void setOrientation(CardputerProgressOrientation orientation);
  void draw(CardputerDisplay& display) override;

 private:
  float ratio() const;

  float value_;
  float min_;
  float max_;
  uint16_t fill_;
  uint16_t stroke_;
  uint16_t background_;
  int radius_;
  CardputerProgressOrientation orientation_;
};
