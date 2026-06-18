#include "sparkline.h"

#include <algorithm>

static float clampf(float value, float min, float max) {
  return std::max(min, std::min(max, value));
}

CardputerSparkline::CardputerSparkline(
    CardputerRect bounds,
    float min,
    float max,
    uint16_t stroke,
    uint16_t axis,
    uint16_t background,
    int thickness,
    bool showAxes)
    : CardputerWidget(bounds),
      min_(min),
      max_(max),
      stroke_(stroke),
      axis_(axis),
      background_(background),
      thickness_(std::max(1, thickness)),
      showAxes_(showAxes) {}

void CardputerSparkline::clear() {
  count_ = 0;
  cursor_ = 0;
  invalidate();
}

void CardputerSparkline::addSample(float value) {
  samples_[cursor_] = clampf(value, min_, max_);
  cursor_ = (cursor_ + 1) % MAX_SAMPLES;
  if (count_ < MAX_SAMPLES) ++count_;
  invalidate();
}

void CardputerSparkline::setSamples(const float* samples, size_t count) {
  clear();
  if (!samples) return;
  const size_t start = count > MAX_SAMPLES ? count - MAX_SAMPLES : 0;
  for (size_t i = start; i < count; ++i) addSample(samples[i]);
}

void CardputerSparkline::setAxesVisible(bool visible) {
  if (showAxes_ == visible) return;
  showAxes_ = visible;
  invalidate();
}

size_t CardputerSparkline::sampleCount() const {
  return count_;
}

float CardputerSparkline::sampleAt(size_t index) const {
  if (index >= count_) return min_;
  const size_t first = count_ == MAX_SAMPLES ? cursor_ : 0;
  return samples_[(first + index) % MAX_SAMPLES];
}

int CardputerSparkline::sampleX(size_t index, size_t count) const {
  if (count <= 1) return bounds_.x;
  return bounds_.x + static_cast<int>((bounds_.w - 1) * index / (count - 1));
}

int CardputerSparkline::sampleY(float value) const {
  const float span = max_ - min_;
  const float ratio = span == 0.0f ? 0.0f : clampf((value - min_) / span, 0.0f, 1.0f);
  return bounds_.y + bounds_.h - 1 - static_cast<int>((bounds_.h - 1) * ratio);
}

void CardputerSparkline::drawThickLine(CardputerDisplay& display, int x0, int y0, int x1, int y1, uint16_t color) const {
  display.drawLine(x0, y0, x1, y1, color);
  for (int offset = 1; offset < thickness_; ++offset) {
    display.drawLine(x0, y0 + offset, x1, y1 + offset, color);
  }
}

void CardputerSparkline::draw(CardputerDisplay& display) {
  display.fillRect(bounds_.x, bounds_.y, bounds_.w, bounds_.h, background_);

  if (showAxes_) {
    display.drawRect(bounds_.x, bounds_.y, bounds_.w, bounds_.h, axis_);
    display.drawLine(bounds_.x, bounds_.y + bounds_.h / 2, bounds_.x + bounds_.w - 1, bounds_.y + bounds_.h / 2, axis_);
    display.drawLine(bounds_.x + bounds_.w / 2, bounds_.y, bounds_.x + bounds_.w / 2, bounds_.y + bounds_.h - 1, axis_);
  }

  const size_t count = sampleCount();
  if (count == 0) return;
  if (count == 1) {
    display.drawPixel(sampleX(0, count), sampleY(sampleAt(0)), stroke_);
    return;
  }

  int prevX = sampleX(0, count);
  int prevY = sampleY(sampleAt(0));
  for (size_t i = 1; i < count; ++i) {
    const int x = sampleX(i, count);
    const int y = sampleY(sampleAt(i));
    drawThickLine(display, prevX, prevY, x, y, stroke_);
    prevX = x;
    prevY = y;
  }
}
