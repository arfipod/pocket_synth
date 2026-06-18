#pragma once

#include <stdint.h>

struct CardputerBitmapFont5x7 {
  const char* name;
  uint8_t firstCodepoint;
  uint8_t lastCodepoint;
  uint8_t glyphWidth;
  uint8_t glyphHeight;
  uint8_t glyphAdvance;
  uint8_t lineHeight;
};

extern const CardputerBitmapFont5x7 CARDPUTER_FONT_5X7;

const uint8_t* cardputerFont5x7Glyph(char ch);
