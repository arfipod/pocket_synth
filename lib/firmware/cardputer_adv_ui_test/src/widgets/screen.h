#pragma once

#include "widget.h"
#include <stddef.h>

class CardputerScreen {
 public:
  void add(CardputerWidget* widget);
  void begin();
  void update(uint32_t nowMs);
  void drawAll(CardputerDisplay& display);
  void flushAll(CardputerDisplay& display);
  void flushDirty(CardputerDisplay& display);

 private:
  static constexpr size_t MAX_WIDGETS = 24;

  bool intersects(CardputerRect a, CardputerRect b) const;
  CardputerRect padded(CardputerRect rect, int amount) const;
  void drawIntersecting(CardputerDisplay& display, CardputerRect rect);

  CardputerWidget* widgets_[MAX_WIDGETS] = {};
  size_t count_ = 0;
};
