#pragma once

#include "esp_err.h"
#include "freertos/FreeRTOS.h"

#include <stddef.h>
#include <stdint.h>

namespace pocketsynth {

inline constexpr int M32_OLED_WIDTH = 128;
inline constexpr int M32_OLED_HEIGHT = 32;
inline constexpr size_t M32_OLED_FRAMEBUFFER_BYTES =
    static_cast<size_t>(M32_OLED_WIDTH * M32_OLED_HEIGHT / 8);

struct M32OledStatus {
  bool enabled;
  bool started;
  bool clientRegistered;
  bool deviceConnected;
  bool interfaceClaimed;
  bool transferActive;
  bool frameQueued;
  bool outputInverted;
  esp_err_t lastError;
  uint32_t connectCount;
  uint32_t disconnectCount;
  uint32_t frameCount;
  uint32_t transferCount;
  uint32_t errorCount;
  uint32_t lastEventMs;
  uint32_t lastFrameMs;
  uint8_t address;
  uint8_t interfaceNumber;
  uint8_t alternateSetting;
  uint8_t endpointAddress;
  uint16_t endpointMaxPacketSize;
  uint16_t vid;
  uint16_t pid;
  char lastEvent[32];
};

class M32OledFramebuffer {
 public:
  M32OledFramebuffer();

  static constexpr int width() { return M32_OLED_WIDTH; }
  static constexpr int height() { return M32_OLED_HEIGHT; }
  static constexpr size_t byteCount() { return M32_OLED_FRAMEBUFFER_BYTES; }

  void clear(bool on = false);
  void invert();

  void setPixel(int x, int y, bool on = true);
  bool getPixel(int x, int y) const;

  void drawHLine(int x, int y, int w, bool on = true);
  void drawVLine(int x, int y, int h, bool on = true);
  void drawLine(int x0, int y0, int x1, int y1, bool on = true);
  void drawRect(int x, int y, int w, int h, bool on = true);
  void fillRect(int x, int y, int w, int h, bool on = true);

  void drawChar(int x, int y, char c, uint8_t scale = 1, bool on = true);
  int drawText(int x, int y, const char* text, uint8_t scale = 1, bool on = true);

  void drawBar(int x, int y, int w, int h, float normalized, bool border = true, bool on = true);
  void drawWaveIcon(int x, int y, int w, int h, bool on = true);
  void drawWaveform(int x, int y, int w, int h, const int8_t* samples, size_t sampleCount, bool on = true);

  const uint8_t* data() const { return bits_; }
  uint8_t* data() { return bits_; }

 private:
  uint8_t bits_[M32_OLED_FRAMEBUFFER_BYTES];
};

bool isM32OledBuildEnabled();
esp_err_t initializeM32Oled();

esp_err_t sendM32OledFrame(const M32OledFramebuffer& frame, TickType_t timeoutTicks = 0);
esp_err_t sendM32OledText(const char* line1,
                          const char* line2 = nullptr,
                          const char* line3 = nullptr,
                          TickType_t timeoutTicks = 0);
esp_err_t sendM32OledParameter(const char* name,
                               const char* value,
                               float normalizedValue,
                               TickType_t timeoutTicks = 0);

void setM32OledOutputInverted(bool inverted);
bool isM32OledOutputInverted();

M32OledStatus getM32OledStatus();
size_t writeM32OledStatusJson(char* out, size_t outSize);

}  // namespace pocketsynth
