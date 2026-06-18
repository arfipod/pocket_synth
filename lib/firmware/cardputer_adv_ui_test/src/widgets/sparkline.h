#pragma once

#include "widget.h"
#include <stddef.h>

class CardputerSparkline : public CardputerWidget {
 public:
  static constexpr size_t MAX_SAMPLES = 96;

  CardputerSparkline(
      CardputerRect bounds,
      float min,
      float max,
      uint16_t stroke,
      uint16_t axis,
      uint16_t background,
      int thickness = 1,
      bool showAxes = false);

  void clear();
  void addSample(float value);
  void setSamples(const float* samples, size_t count);
  void setAxesVisible(bool visible);
  void draw(CardputerDisplay& display) override;

 private:
  size_t sampleCount() const;
  float sampleAt(size_t index) const;
  int sampleX(size_t index, size_t count) const;
  int sampleY(float value) const;
  void drawThickLine(CardputerDisplay& display, int x0, int y0, int x1, int y1, uint16_t color) const;

  float samples_[MAX_SAMPLES] = {};
  size_t count_ = 0;
  size_t cursor_ = 0;
  float min_;
  float max_;
  uint16_t stroke_;
  uint16_t axis_;
  uint16_t background_;
  int thickness_;
  bool showAxes_;
};
