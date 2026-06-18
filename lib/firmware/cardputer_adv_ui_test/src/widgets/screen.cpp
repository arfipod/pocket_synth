#include "screen.h"

void CardputerScreen::add(CardputerWidget* widget) {
  if (count_ >= MAX_WIDGETS || widget == nullptr) return;
  widgets_[count_++] = widget;
}

void CardputerScreen::begin() {
  for (size_t i = 0; i < count_; ++i) widgets_[i]->begin();
}

void CardputerScreen::update(uint32_t nowMs) {
  for (size_t i = 0; i < count_; ++i) widgets_[i]->update(nowMs);
}

void CardputerScreen::drawAll(CardputerDisplay& display) {
  for (size_t i = 0; i < count_; ++i) {
    if (widgets_[i]->visible()) widgets_[i]->draw(display);
    widgets_[i]->markClean();
  }
}

void CardputerScreen::flushAll(CardputerDisplay& display) {
  drawAll(display);
  display.flush();
}

void CardputerScreen::flushDirty(CardputerDisplay& display) {
  CardputerRect dirtyRects[MAX_WIDGETS] = {};
  size_t dirtyCount = 0;

  for (size_t i = 0; i < count_; ++i) {
    if (!widgets_[i]->dirty()) continue;
    dirtyRects[dirtyCount++] = padded(widgets_[i]->bounds(), 2);
  }

  for (size_t i = 0; i < dirtyCount; ++i) {
    drawIntersecting(display, dirtyRects[i]);
    display.flushRect(dirtyRects[i]);
  }

  for (size_t i = 0; i < count_; ++i) widgets_[i]->markClean();
}

bool CardputerScreen::intersects(CardputerRect a, CardputerRect b) const {
  return a.x < b.x + b.w && a.x + a.w > b.x && a.y < b.y + b.h && a.y + a.h > b.y;
}

CardputerRect CardputerScreen::padded(CardputerRect rect, int amount) const {
  return {rect.x - amount, rect.y - amount, rect.w + amount * 2, rect.h + amount * 2};
}

void CardputerScreen::drawIntersecting(CardputerDisplay& display, CardputerRect rect) {
  for (size_t i = 0; i < count_; ++i) {
    if (widgets_[i]->visible() && intersects(widgets_[i]->bounds(), rect)) {
      widgets_[i]->draw(display);
    }
  }
}
