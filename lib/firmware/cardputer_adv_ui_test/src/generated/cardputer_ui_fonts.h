#pragma once

#include "../cardputer_display.h"
#include <stdint.h>

enum CardputerTextAlign { CARDPUTER_ALIGN_LEFT, CARDPUTER_ALIGN_CENTER, CARDPUTER_ALIGN_RIGHT };

struct CardputerGeneratedGlyph { uint32_t codepoint; uint16_t offset; uint8_t width; uint8_t height; int8_t x_offset; int8_t y_offset; uint8_t x_advance; };
struct CardputerGeneratedFont { const char* name; uint8_t size; const uint8_t* bitmap; uint16_t bitmap_size; const CardputerGeneratedGlyph* glyphs; uint16_t glyph_count; };


void drawGeneratedText(CardputerDisplay& display, const CardputerGeneratedFont* font, const char* text, int x, int y, uint16_t color, CardputerTextAlign align);
