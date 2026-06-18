#pragma once

#include "../cardputer_display.h"
#include <stdint.h>

enum class CardputerTextAlign {
  Left,
  Center,
  Right,
};

class CardputerWidget {
 public:
  explicit CardputerWidget(CardputerRect bounds);
  virtual ~CardputerWidget() = default;

  virtual void begin() {}
  virtual void update(uint32_t nowMs) { (void)nowMs; }
  virtual void draw(CardputerDisplay& display) = 0;

  CardputerRect bounds() const { return bounds_; }
  bool dirty() const { return dirty_; }
  bool visible() const { return visible_; }
  void setVisible(bool visible);
  void invalidate() { dirty_ = true; }
  void markClean() { dirty_ = false; }

 protected:
  CardputerRect bounds_;
  bool visible_ = true;
  bool dirty_ = true;
};
