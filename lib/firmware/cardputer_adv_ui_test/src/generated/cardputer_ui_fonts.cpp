#include "cardputer_ui_fonts.h"

static const CardputerGeneratedGlyph* find_glyph(const CardputerGeneratedFont* font, uint32_t codepoint) {
  if (!font) return nullptr;
  for (uint16_t i = 0; i < font->glyph_count; ++i) if (font->glyphs[i].codepoint == codepoint) return &font->glyphs[i];
  return nullptr;
}

static uint32_t read_utf8_codepoint(const unsigned char*& p) {
  uint32_t c = *p++;
  if ((c & 0x80) == 0) return c;
  if ((c & 0xE0) == 0xC0 && *p) return ((c & 0x1F) << 6) | (*p++ & 0x3F);
  if ((c & 0xF0) == 0xE0 && p[0] && p[1]) { uint32_t c1 = *p++; uint32_t c2 = *p++; return ((c & 0x0F) << 12) | ((c1 & 0x3F) << 6) | (c2 & 0x3F); }
  if ((c & 0xF8) == 0xF0 && p[0] && p[1] && p[2]) { uint32_t c1 = *p++; uint32_t c2 = *p++; uint32_t c3 = *p++; return ((c & 0x07) << 18) | ((c1 & 0x3F) << 12) | ((c2 & 0x3F) << 6) | (c3 & 0x3F); }
  return c;
}

static int measure_text(const CardputerGeneratedFont* font, const char* text) {
  int width = 0;
  const unsigned char* p = reinterpret_cast<const unsigned char*>(text);
  while (*p) {
    const CardputerGeneratedGlyph* glyph = find_glyph(font, read_utf8_codepoint(p));
    width += glyph ? glyph->x_advance : font->size / 2;
  }
  return width;
}

void drawGeneratedText(CardputerDisplay& display, const CardputerGeneratedFont* font, const char* text, int x, int y, uint16_t color, CardputerTextAlign align) {
  if (!font || !text) return;
  int cursor = x;
  const int total = measure_text(font, text);
  if (align == CARDPUTER_ALIGN_CENTER) cursor -= total / 2;
  if (align == CARDPUTER_ALIGN_RIGHT) cursor -= total;
  const int baseline = y + font->size / 3;
  const unsigned char* p = reinterpret_cast<const unsigned char*>(text);
  while (*p) {
    const CardputerGeneratedGlyph* glyph = find_glyph(font, read_utf8_codepoint(p));
    if (!glyph) { cursor += font->size / 2; continue; }
    for (uint8_t gy = 0; gy < glyph->height; ++gy) {
      for (uint8_t gx = 0; gx < glyph->width; ++gx) {
        const uint16_t bit = gy * glyph->width + gx;
        const uint8_t mask = 0x80 >> (bit & 7);
        if (font->bitmap[glyph->offset + (bit >> 3)] & mask) display.drawPixel(cursor + glyph->x_offset + gx, baseline + glyph->y_offset + gy, color);
      }
    }
    cursor += glyph->x_advance;
  }
}
