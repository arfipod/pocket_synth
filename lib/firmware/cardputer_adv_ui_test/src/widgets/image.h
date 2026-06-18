#pragma once

#include "widget.h"
#include <stddef.h>

class CardputerImage : public CardputerWidget {
 public:
  CardputerImage(CardputerRect bounds, const uint16_t* pixels, int imageWidth, int imageHeight, uint16_t fallbackStroke, uint16_t fallbackFill);
  void setBitmap(const uint16_t* pixels, int imageWidth, int imageHeight);
  void draw(CardputerDisplay& display) override;

 private:
  const uint16_t* pixels_;
  int imageWidth_;
  int imageHeight_;
  uint16_t fallbackStroke_;
  uint16_t fallbackFill_;
};
