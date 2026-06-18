#pragma once

#include "widget.h"

enum class CardputerIconKind {
  Wifi,
  Sd,
  Battery,
  Audio,
  Imu,
  Play,
  Pause,
  Warning,
};

class CardputerIcon : public CardputerWidget {
 public:
  CardputerIcon(CardputerRect bounds, CardputerIconKind kind, uint16_t color);
  void setKind(CardputerIconKind kind);
  void setColor(uint16_t color);
  void draw(CardputerDisplay& display) override;

 private:
  void drawWifi(CardputerDisplay& display, int cx, int cy, int size) const;
  void drawSd(CardputerDisplay& display) const;
  void drawBattery(CardputerDisplay& display) const;
  void drawAudio(CardputerDisplay& display, int cx, int cy, int size) const;
  void drawImu(CardputerDisplay& display) const;
  void drawPlay(CardputerDisplay& display) const;
  void drawPause(CardputerDisplay& display) const;
  void drawWarning(CardputerDisplay& display) const;

  CardputerIconKind kind_;
  uint16_t color_;
};
