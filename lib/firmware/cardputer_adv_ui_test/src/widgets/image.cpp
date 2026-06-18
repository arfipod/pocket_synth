#include "image.h"

#include <algorithm>

CardputerImage::CardputerImage(CardputerRect bounds, const uint16_t* pixels, int imageWidth, int imageHeight, uint16_t fallbackStroke, uint16_t fallbackFill)
    : CardputerWidget(bounds),
      pixels_(pixels),
      imageWidth_(imageWidth),
      imageHeight_(imageHeight),
      fallbackStroke_(fallbackStroke),
      fallbackFill_(fallbackFill) {}

void CardputerImage::setBitmap(const uint16_t* pixels, int imageWidth, int imageHeight) {
  pixels_ = pixels;
  imageWidth_ = imageWidth;
  imageHeight_ = imageHeight;
  invalidate();
}

void CardputerImage::draw(CardputerDisplay& display) {
  if (!pixels_ || imageWidth_ <= 0 || imageHeight_ <= 0) {
    display.fillRect(bounds_.x, bounds_.y, bounds_.w, bounds_.h, fallbackFill_);
    display.drawRect(bounds_.x, bounds_.y, bounds_.w, bounds_.h, fallbackStroke_);
    display.drawLine(bounds_.x, bounds_.y, bounds_.x + bounds_.w - 1, bounds_.y + bounds_.h - 1, fallbackStroke_);
    display.drawLine(bounds_.x + bounds_.w - 1, bounds_.y, bounds_.x, bounds_.y + bounds_.h - 1, fallbackStroke_);
    return;
  }

  for (int y = 0; y < bounds_.h; ++y) {
    const int sy = std::min(imageHeight_ - 1, y * imageHeight_ / bounds_.h);
    for (int x = 0; x < bounds_.w; ++x) {
      const int sx = std::min(imageWidth_ - 1, x * imageWidth_ / bounds_.w);
      display.drawPixel(bounds_.x + x, bounds_.y + y, pixels_[sy * imageWidth_ + sx]);
    }
  }
}
