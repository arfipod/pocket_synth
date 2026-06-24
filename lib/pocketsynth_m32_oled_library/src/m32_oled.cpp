#include "m32_oled.h"

#include "boot_diagnostics.h"
#include "synth_config.h"
#include "usb_host_runtime.h"

#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef POCKETSYNTH_ENABLE_M32_OLED
#define POCKETSYNTH_ENABLE_M32_OLED 0
#endif

#if POCKETSYNTH_ENABLE_M32_OLED
#include "usb/usb_host.h"
#endif

namespace pocketsynth {
namespace {

constexpr const char* TAG = "m32_oled";
constexpr uint16_t NATIVE_INSTRUMENTS_VID = 0x17cc;
constexpr uint16_t KOMPLETE_KONTROL_M32_PID = 0x1860;
constexpr uint8_t USB_CLASS_HID_LOCAL = 0x03;
constexpr uint8_t M32_OLED_REPORT_ID = 0xe0;
constexpr size_t M32_OLED_REPORT_BYTES = 265;
constexpr size_t M32_OLED_REGION_HEADER_BYTES = 9;
constexpr uint16_t M32_OLED_REGION_WIDTH = 128;
constexpr uint16_t M32_OLED_REGION_HEIGHT_PAGES = 2;
constexpr size_t M32_OLED_REGION_PAYLOAD_BYTES =
    static_cast<size_t>(M32_OLED_REGION_WIDTH * M32_OLED_REGION_HEIGHT_PAGES);
constexpr uint8_t M32_OLED_QUEUE_DEPTH = 2;

struct M32OledQueuedFrame {
  uint8_t bits[M32_OLED_FRAMEBUFFER_BYTES];
};

struct M32OledInterfaceInfo {
  bool found;
  uint8_t interfaceNumber;
  uint8_t alternateSetting;
  uint8_t endpointAddress;
  uint8_t endpointAttributes;
  uint16_t endpointMaxPacketSize;
};

#if POCKETSYNTH_ENABLE_M32_OLED
struct TrackedM32OledDevice {
  uint8_t address;
  usb_device_handle_t handle;
  usb_transfer_t* transfer;
  M32OledInterfaceInfo hid;
  bool connected;
  bool interfaceClaimed;
  bool transferInFlight;
  bool hasCurrentFrame;
  uint8_t currentSegment;
  M32OledQueuedFrame currentFrame;
  uint16_t vid;
  uint16_t pid;
  uint8_t actions;
};

enum M32OledAction : uint8_t {
  M32OledActionOpen = 1U << 0,
  M32OledActionClose = 1U << 1,
  M32OledActionSubmitTransfer = 1U << 2,
  M32OledActionClearEndpoint = 1U << 3,
};
#endif

struct JsonWriter {
  char* out;
  size_t outSize;
  size_t used;
  bool truncated;
};

portMUX_TYPE gM32OledMux = portMUX_INITIALIZER_UNLOCKED;
M32OledStatus gStatus = {};
bool gOutputInverted = true;
QueueHandle_t gFrameQueue = nullptr;

#if POCKETSYNTH_ENABLE_M32_OLED
usb_host_client_handle_t gClientHandle = nullptr;
TrackedM32OledDevice gDevice = {};
bool gInitAttempted = false;
bool gHasPendingActions = false;
#endif

void copyString(char* out, size_t outSize, const char* in) {
  if (out == nullptr || outSize == 0) {
    return;
  }
  snprintf(out, outSize, "%s", in != nullptr ? in : "");
}

uint32_t millis() {
  return static_cast<uint32_t>(esp_timer_get_time() / 1000ULL);
}

void setLastEventUnlocked(M32OledStatus* status, const char* eventName, esp_err_t err) {
  if (status == nullptr) {
    return;
  }
  status->lastError = err;
  status->lastEventMs = millis();
  copyString(status->lastEvent, sizeof(status->lastEvent), eventName);
}

void publishStarted() {
  portENTER_CRITICAL(&gM32OledMux);
  gStatus.started = true;
  setLastEventUnlocked(&gStatus, "starting", ESP_OK);
  portEXIT_CRITICAL(&gM32OledMux);
}

void publishClientRegistered(esp_err_t err) {
  portENTER_CRITICAL(&gM32OledMux);
  gStatus.clientRegistered = err == ESP_OK;
  setLastEventUnlocked(&gStatus, err == ESP_OK ? "client_ready" : "client_error", err);
  if (err != ESP_OK) {
    ++gStatus.errorCount;
  }
  portEXIT_CRITICAL(&gM32OledMux);
}

void publishError(const char* eventName, esp_err_t err) {
  portENTER_CRITICAL(&gM32OledMux);
  ++gStatus.errorCount;
  setLastEventUnlocked(&gStatus, eventName, err);
  portEXIT_CRITICAL(&gM32OledMux);
}

void publishQueueState(bool hasQueuedFrame) {
  portENTER_CRITICAL(&gM32OledMux);
  gStatus.frameQueued = hasQueuedFrame;
  portEXIT_CRITICAL(&gM32OledMux);
}

#if POCKETSYNTH_ENABLE_M32_OLED
void publishConnected(const TrackedM32OledDevice& device) {
  portENTER_CRITICAL(&gM32OledMux);
  gStatus.deviceConnected = true;
  gStatus.interfaceClaimed = device.interfaceClaimed;
  gStatus.transferActive = device.transferInFlight;
  gStatus.address = device.address;
  gStatus.interfaceNumber = device.hid.interfaceNumber;
  gStatus.alternateSetting = device.hid.alternateSetting;
  gStatus.endpointAddress = device.hid.endpointAddress;
  gStatus.endpointMaxPacketSize = device.hid.endpointMaxPacketSize;
  gStatus.vid = device.vid;
  gStatus.pid = device.pid;
  gStatus.outputInverted = gOutputInverted;
  ++gStatus.connectCount;
  setLastEventUnlocked(&gStatus, "connected", ESP_OK);
  portEXIT_CRITICAL(&gM32OledMux);
}

void publishDisconnected(uint8_t address) {
  portENTER_CRITICAL(&gM32OledMux);
  gStatus.deviceConnected = false;
  gStatus.interfaceClaimed = false;
  gStatus.transferActive = false;
  gStatus.frameQueued = false;
  gStatus.address = address;
  ++gStatus.disconnectCount;
  setLastEventUnlocked(&gStatus, "disconnected", ESP_OK);
  portEXIT_CRITICAL(&gM32OledMux);
}

void publishTransferActive(bool active, esp_err_t err) {
  portENTER_CRITICAL(&gM32OledMux);
  gStatus.transferActive = active;
  if (active) {
    ++gStatus.transferCount;
    setLastEventUnlocked(&gStatus, "transfer", ESP_OK);
  } else if (err != ESP_OK) {
    ++gStatus.errorCount;
    setLastEventUnlocked(&gStatus, "transfer_error", err);
  }
  portEXIT_CRITICAL(&gM32OledMux);
}

void publishFrameSent() {
  const bool hasQueuedFrame = gFrameQueue != nullptr && uxQueueMessagesWaiting(gFrameQueue) > 0;
  portENTER_CRITICAL(&gM32OledMux);
  ++gStatus.frameCount;
  gStatus.lastFrameMs = millis();
  gStatus.frameQueued = hasQueuedFrame;
  setLastEventUnlocked(&gStatus, "frame_sent", ESP_OK);
  portEXIT_CRITICAL(&gM32OledMux);
}
#endif

void initializeStatusDefaults() {
  portENTER_CRITICAL(&gM32OledMux);
  gStatus = {};
  gStatus.enabled = isM32OledBuildEnabled();
  gStatus.outputInverted = gOutputInverted;
  gStatus.lastError = ESP_ERR_INVALID_STATE;
  copyString(gStatus.lastEvent, sizeof(gStatus.lastEvent), "idle");
  portEXIT_CRITICAL(&gM32OledMux);
}

uint8_t normalizeChar(char c) {
  if (c >= 'a' && c <= 'z') {
    return static_cast<uint8_t>(c - 'a' + 'A');
  }
  return static_cast<uint8_t>(c);
}

void glyphRows(char c, uint8_t rows[7]) {
  memset(rows, 0, 7);
  switch (normalizeChar(c)) {
    case ' ': break;
    case '!': { const uint8_t r[7] = {0x04,0x04,0x04,0x04,0x04,0x00,0x04}; memcpy(rows,r,7); break; }
    case '"': { const uint8_t r[7] = {0x0a,0x0a,0x0a,0x00,0x00,0x00,0x00}; memcpy(rows,r,7); break; }
    case '#': { const uint8_t r[7] = {0x0a,0x0a,0x1f,0x0a,0x1f,0x0a,0x0a}; memcpy(rows,r,7); break; }
    case '$': { const uint8_t r[7] = {0x04,0x0f,0x14,0x0e,0x05,0x1e,0x04}; memcpy(rows,r,7); break; }
    case '%': { const uint8_t r[7] = {0x18,0x19,0x02,0x04,0x08,0x13,0x03}; memcpy(rows,r,7); break; }
    case '&': { const uint8_t r[7] = {0x0c,0x12,0x14,0x08,0x15,0x12,0x0d}; memcpy(rows,r,7); break; }
    case '\'': { const uint8_t r[7] = {0x04,0x04,0x08,0x00,0x00,0x00,0x00}; memcpy(rows,r,7); break; }
    case '(': { const uint8_t r[7] = {0x02,0x04,0x08,0x08,0x08,0x04,0x02}; memcpy(rows,r,7); break; }
    case ')': { const uint8_t r[7] = {0x08,0x04,0x02,0x02,0x02,0x04,0x08}; memcpy(rows,r,7); break; }
    case '*': { const uint8_t r[7] = {0x00,0x04,0x15,0x0e,0x15,0x04,0x00}; memcpy(rows,r,7); break; }
    case '+': { const uint8_t r[7] = {0x00,0x04,0x04,0x1f,0x04,0x04,0x00}; memcpy(rows,r,7); break; }
    case ',': { const uint8_t r[7] = {0x00,0x00,0x00,0x00,0x04,0x04,0x08}; memcpy(rows,r,7); break; }
    case '-': { const uint8_t r[7] = {0x00,0x00,0x00,0x1f,0x00,0x00,0x00}; memcpy(rows,r,7); break; }
    case '.': { const uint8_t r[7] = {0x00,0x00,0x00,0x00,0x00,0x0c,0x0c}; memcpy(rows,r,7); break; }
    case '/': { const uint8_t r[7] = {0x01,0x02,0x02,0x04,0x08,0x08,0x10}; memcpy(rows,r,7); break; }
    case '0': { const uint8_t r[7] = {0x0e,0x11,0x13,0x15,0x19,0x11,0x0e}; memcpy(rows,r,7); break; }
    case '1': { const uint8_t r[7] = {0x04,0x0c,0x04,0x04,0x04,0x04,0x0e}; memcpy(rows,r,7); break; }
    case '2': { const uint8_t r[7] = {0x0e,0x11,0x01,0x02,0x04,0x08,0x1f}; memcpy(rows,r,7); break; }
    case '3': { const uint8_t r[7] = {0x1f,0x02,0x04,0x02,0x01,0x11,0x0e}; memcpy(rows,r,7); break; }
    case '4': { const uint8_t r[7] = {0x02,0x06,0x0a,0x12,0x1f,0x02,0x02}; memcpy(rows,r,7); break; }
    case '5': { const uint8_t r[7] = {0x1f,0x10,0x1e,0x01,0x01,0x11,0x0e}; memcpy(rows,r,7); break; }
    case '6': { const uint8_t r[7] = {0x06,0x08,0x10,0x1e,0x11,0x11,0x0e}; memcpy(rows,r,7); break; }
    case '7': { const uint8_t r[7] = {0x1f,0x01,0x02,0x04,0x08,0x08,0x08}; memcpy(rows,r,7); break; }
    case '8': { const uint8_t r[7] = {0x0e,0x11,0x11,0x0e,0x11,0x11,0x0e}; memcpy(rows,r,7); break; }
    case '9': { const uint8_t r[7] = {0x0e,0x11,0x11,0x0f,0x01,0x02,0x0c}; memcpy(rows,r,7); break; }
    case ':': { const uint8_t r[7] = {0x00,0x0c,0x0c,0x00,0x0c,0x0c,0x00}; memcpy(rows,r,7); break; }
    case ';': { const uint8_t r[7] = {0x00,0x0c,0x0c,0x00,0x04,0x04,0x08}; memcpy(rows,r,7); break; }
    case '<': { const uint8_t r[7] = {0x02,0x04,0x08,0x10,0x08,0x04,0x02}; memcpy(rows,r,7); break; }
    case '=': { const uint8_t r[7] = {0x00,0x00,0x1f,0x00,0x1f,0x00,0x00}; memcpy(rows,r,7); break; }
    case '>': { const uint8_t r[7] = {0x08,0x04,0x02,0x01,0x02,0x04,0x08}; memcpy(rows,r,7); break; }
    case '?': { const uint8_t r[7] = {0x0e,0x11,0x01,0x02,0x04,0x00,0x04}; memcpy(rows,r,7); break; }
    case '@': { const uint8_t r[7] = {0x0e,0x11,0x01,0x0d,0x15,0x15,0x0e}; memcpy(rows,r,7); break; }
    case 'A': { const uint8_t r[7] = {0x0e,0x11,0x11,0x1f,0x11,0x11,0x11}; memcpy(rows,r,7); break; }
    case 'B': { const uint8_t r[7] = {0x1e,0x11,0x11,0x1e,0x11,0x11,0x1e}; memcpy(rows,r,7); break; }
    case 'C': { const uint8_t r[7] = {0x0e,0x11,0x10,0x10,0x10,0x11,0x0e}; memcpy(rows,r,7); break; }
    case 'D': { const uint8_t r[7] = {0x1e,0x11,0x11,0x11,0x11,0x11,0x1e}; memcpy(rows,r,7); break; }
    case 'E': { const uint8_t r[7] = {0x1f,0x10,0x10,0x1e,0x10,0x10,0x1f}; memcpy(rows,r,7); break; }
    case 'F': { const uint8_t r[7] = {0x1f,0x10,0x10,0x1e,0x10,0x10,0x10}; memcpy(rows,r,7); break; }
    case 'G': { const uint8_t r[7] = {0x0e,0x11,0x10,0x17,0x11,0x11,0x0f}; memcpy(rows,r,7); break; }
    case 'H': { const uint8_t r[7] = {0x11,0x11,0x11,0x1f,0x11,0x11,0x11}; memcpy(rows,r,7); break; }
    case 'I': { const uint8_t r[7] = {0x0e,0x04,0x04,0x04,0x04,0x04,0x0e}; memcpy(rows,r,7); break; }
    case 'J': { const uint8_t r[7] = {0x07,0x02,0x02,0x02,0x12,0x12,0x0c}; memcpy(rows,r,7); break; }
    case 'K': { const uint8_t r[7] = {0x11,0x12,0x14,0x18,0x14,0x12,0x11}; memcpy(rows,r,7); break; }
    case 'L': { const uint8_t r[7] = {0x10,0x10,0x10,0x10,0x10,0x10,0x1f}; memcpy(rows,r,7); break; }
    case 'M': { const uint8_t r[7] = {0x11,0x1b,0x15,0x15,0x11,0x11,0x11}; memcpy(rows,r,7); break; }
    case 'N': { const uint8_t r[7] = {0x11,0x19,0x15,0x13,0x11,0x11,0x11}; memcpy(rows,r,7); break; }
    case 'O': { const uint8_t r[7] = {0x0e,0x11,0x11,0x11,0x11,0x11,0x0e}; memcpy(rows,r,7); break; }
    case 'P': { const uint8_t r[7] = {0x1e,0x11,0x11,0x1e,0x10,0x10,0x10}; memcpy(rows,r,7); break; }
    case 'Q': { const uint8_t r[7] = {0x0e,0x11,0x11,0x11,0x15,0x12,0x0d}; memcpy(rows,r,7); break; }
    case 'R': { const uint8_t r[7] = {0x1e,0x11,0x11,0x1e,0x14,0x12,0x11}; memcpy(rows,r,7); break; }
    case 'S': { const uint8_t r[7] = {0x0f,0x10,0x10,0x0e,0x01,0x01,0x1e}; memcpy(rows,r,7); break; }
    case 'T': { const uint8_t r[7] = {0x1f,0x04,0x04,0x04,0x04,0x04,0x04}; memcpy(rows,r,7); break; }
    case 'U': { const uint8_t r[7] = {0x11,0x11,0x11,0x11,0x11,0x11,0x0e}; memcpy(rows,r,7); break; }
    case 'V': { const uint8_t r[7] = {0x11,0x11,0x11,0x11,0x11,0x0a,0x04}; memcpy(rows,r,7); break; }
    case 'W': { const uint8_t r[7] = {0x11,0x11,0x11,0x15,0x15,0x15,0x0a}; memcpy(rows,r,7); break; }
    case 'X': { const uint8_t r[7] = {0x11,0x11,0x0a,0x04,0x0a,0x11,0x11}; memcpy(rows,r,7); break; }
    case 'Y': { const uint8_t r[7] = {0x11,0x11,0x0a,0x04,0x04,0x04,0x04}; memcpy(rows,r,7); break; }
    case 'Z': { const uint8_t r[7] = {0x1f,0x01,0x02,0x04,0x08,0x10,0x1f}; memcpy(rows,r,7); break; }
    case '[': { const uint8_t r[7] = {0x0e,0x08,0x08,0x08,0x08,0x08,0x0e}; memcpy(rows,r,7); break; }
    case '\\': { const uint8_t r[7] = {0x10,0x08,0x08,0x04,0x02,0x02,0x01}; memcpy(rows,r,7); break; }
    case ']': { const uint8_t r[7] = {0x0e,0x02,0x02,0x02,0x02,0x02,0x0e}; memcpy(rows,r,7); break; }
    case '^': { const uint8_t r[7] = {0x04,0x0a,0x11,0x00,0x00,0x00,0x00}; memcpy(rows,r,7); break; }
    case '_': { const uint8_t r[7] = {0x00,0x00,0x00,0x00,0x00,0x00,0x1f}; memcpy(rows,r,7); break; }
    case '`': { const uint8_t r[7] = {0x08,0x04,0x02,0x00,0x00,0x00,0x00}; memcpy(rows,r,7); break; }
    case '{': { const uint8_t r[7] = {0x02,0x04,0x04,0x08,0x04,0x04,0x02}; memcpy(rows,r,7); break; }
    case '|': { const uint8_t r[7] = {0x04,0x04,0x04,0x00,0x04,0x04,0x04}; memcpy(rows,r,7); break; }
    case '}': { const uint8_t r[7] = {0x08,0x04,0x04,0x02,0x04,0x04,0x08}; memcpy(rows,r,7); break; }
    case '~': { const uint8_t r[7] = {0x00,0x00,0x08,0x15,0x02,0x00,0x00}; memcpy(rows,r,7); break; }
    default: { const uint8_t r[7] = {0x0e,0x11,0x01,0x02,0x04,0x00,0x04}; memcpy(rows,r,7); break; }
  }
}

void writeU16Le(uint8_t* out, uint16_t value) {
  out[0] = static_cast<uint8_t>(value & 0xffU);
  out[1] = static_cast<uint8_t>((value >> 8U) & 0xffU);
}

void buildOledRegionReport(uint8_t* out, const M32OledQueuedFrame& frame, uint16_t page) {
  memset(out, 0, M32_OLED_REPORT_BYTES);
  out[0] = M32_OLED_REPORT_ID;
  writeU16Le(out + 1, 0);
  writeU16Le(out + 3, page);
  writeU16Le(out + 5, M32_OLED_REGION_WIDTH);
  writeU16Le(out + 7, M32_OLED_REGION_HEIGHT_PAGES);

  size_t cursor = M32_OLED_REGION_HEADER_BYTES;
  bool invert = false;
  portENTER_CRITICAL(&gM32OledMux);
  invert = gOutputInverted;
  portEXIT_CRITICAL(&gM32OledMux);

  for (int localPage = 0; localPage < static_cast<int>(M32_OLED_REGION_HEIGHT_PAGES); ++localPage) {
    for (int x = 0; x < M32_OLED_WIDTH; ++x) {
      uint8_t packed = 0;
      for (int bit = 0; bit < 8; ++bit) {
        const int y = static_cast<int>((page + localPage) * 8U) + bit;
        const size_t srcIndex = static_cast<size_t>(y * M32_OLED_WIDTH + x);
        const uint8_t srcByte = frame.bits[srcIndex / 8U];
        const bool on = (srcByte & (0x80U >> (srcIndex & 7U))) != 0;
        if (on) {
          packed |= static_cast<uint8_t>(1U << bit);
        }
      }
      out[cursor++] = invert ? static_cast<uint8_t>(~packed) : packed;
    }
  }
}

void appendFormat(JsonWriter* writer, const char* format, ...) {
  if (writer == nullptr || writer->out == nullptr || writer->outSize == 0) {
    return;
  }
  if (writer->used >= writer->outSize) {
    writer->truncated = true;
    writer->out[writer->outSize - 1] = '\0';
    return;
  }

  va_list args;
  va_start(args, format);
  const int written = vsnprintf(writer->out + writer->used, writer->outSize - writer->used, format, args);
  va_end(args);

  if (written <= 0) {
    return;
  }
  const size_t advanced = static_cast<size_t>(written);
  if (advanced >= writer->outSize - writer->used) {
    writer->used = writer->outSize - 1;
    writer->out[writer->used] = '\0';
    writer->truncated = true;
    return;
  }
  writer->used += advanced;
}

void appendJsonString(JsonWriter* writer, const char* text) {
  appendFormat(writer, "\"");
  const char* cursor = text != nullptr ? text : "";
  while (*cursor != '\0') {
    const char ch = *cursor++;
    switch (ch) {
      case '\\': appendFormat(writer, "\\\\"); break;
      case '"': appendFormat(writer, "\\\""); break;
      case '\n': appendFormat(writer, "\\n"); break;
      case '\r': appendFormat(writer, "\\r"); break;
      case '\t': appendFormat(writer, "\\t"); break;
      default:
        if (static_cast<unsigned char>(ch) < 0x20) {
          appendFormat(writer, "\\u%04x", static_cast<unsigned int>(static_cast<unsigned char>(ch)));
        } else {
          appendFormat(writer, "%c", ch);
        }
        break;
    }
  }
  appendFormat(writer, "\"");
}

void appendStatusJson(JsonWriter* writer, const M32OledStatus& status) {
  appendFormat(writer,
               "{"
               "\"enabled\":%s,"
               "\"started\":%s,"
               "\"client_registered\":%s,"
               "\"device_connected\":%s,"
               "\"interface_claimed\":%s,"
               "\"transfer_active\":%s,"
               "\"frame_queued\":%s,"
               "\"output_inverted\":%s,"
               "\"connect_count\":%lu,"
               "\"disconnect_count\":%lu,"
               "\"frame_count\":%lu,"
               "\"transfer_count\":%lu,"
               "\"error_count\":%lu,"
               "\"last_event\":",
               status.enabled ? "true" : "false",
               status.started ? "true" : "false",
               status.clientRegistered ? "true" : "false",
               status.deviceConnected ? "true" : "false",
               status.interfaceClaimed ? "true" : "false",
               status.transferActive ? "true" : "false",
               status.frameQueued ? "true" : "false",
               status.outputInverted ? "true" : "false",
               static_cast<unsigned long>(status.connectCount),
               static_cast<unsigned long>(status.disconnectCount),
               static_cast<unsigned long>(status.frameCount),
               static_cast<unsigned long>(status.transferCount),
               static_cast<unsigned long>(status.errorCount));
  appendJsonString(writer, status.lastEvent);
  appendFormat(writer,
               ",\"last_event_ms\":%lu,"
               "\"last_frame_ms\":%lu,"
               "\"last_error\":\"%s\","
               "\"address\":%u,"
               "\"vid_hex\":\"0x%04x\","
               "\"pid_hex\":\"0x%04x\","
               "\"interface\":%u,"
               "\"alternate_setting\":%u,"
               "\"endpoint_address\":\"0x%02x\","
               "\"endpoint_max_packet_size\":%u"
               "}",
               static_cast<unsigned long>(status.lastEventMs),
               static_cast<unsigned long>(status.lastFrameMs),
               esp_err_to_name(status.lastError),
               static_cast<unsigned int>(status.address),
               static_cast<unsigned int>(status.vid),
               static_cast<unsigned int>(status.pid),
               static_cast<unsigned int>(status.interfaceNumber),
               static_cast<unsigned int>(status.alternateSetting),
               static_cast<unsigned int>(status.endpointAddress),
               static_cast<unsigned int>(status.endpointMaxPacketSize));
}

#if POCKETSYNTH_ENABLE_M32_OLED
const char* transferStatusToString(usb_transfer_status_t status) {
  switch (status) {
    case USB_TRANSFER_STATUS_COMPLETED: return "completed";
    case USB_TRANSFER_STATUS_ERROR: return "error";
    case USB_TRANSFER_STATUS_TIMED_OUT: return "timed_out";
    case USB_TRANSFER_STATUS_CANCELED: return "canceled";
    case USB_TRANSFER_STATUS_STALL: return "stall";
    case USB_TRANSFER_STATUS_OVERFLOW: return "overflow";
    case USB_TRANSFER_STATUS_SKIPPED: return "skipped";
    case USB_TRANSFER_STATUS_NO_DEVICE: return "no_device";
    default: return "unknown";
  }
}

uint16_t endpointMaxPacketSize(const usb_ep_desc_t* endpointDesc) {
  if (endpointDesc == nullptr) {
    return 0;
  }
  return endpointDesc->wMaxPacketSize & USB_W_MAX_PACKET_SIZE_MPS_MASK;
}

bool endpointIsSupportedHidOut(const usb_ep_desc_t* endpointDesc) {
  if (endpointDesc == nullptr) {
    return false;
  }
  const bool isOut = (endpointDesc->bEndpointAddress & USB_B_ENDPOINT_ADDRESS_EP_DIR_MASK) == 0;
  const uint8_t transferType = endpointDesc->bmAttributes & USB_BM_ATTRIBUTES_XFERTYPE_MASK;
  return isOut &&
         (transferType == USB_BM_ATTRIBUTES_XFER_INT || transferType == USB_BM_ATTRIBUTES_XFER_BULK);
}

bool parseM32HidInterface(const usb_config_desc_t* configDesc, M32OledInterfaceInfo* out) {
  if (configDesc == nullptr || out == nullptr) {
    return false;
  }

  *out = {};
  bool currentIsHid = false;
  M32OledInterfaceInfo currentInterface = {};

  int offset = 0;
  const usb_standard_desc_t* descriptor = reinterpret_cast<const usb_standard_desc_t*>(configDesc);
  while ((descriptor = usb_parse_next_descriptor(descriptor, configDesc->wTotalLength, &offset)) != nullptr) {
    if (descriptor->bDescriptorType == USB_B_DESCRIPTOR_TYPE_INTERFACE &&
        descriptor->bLength >= USB_INTF_DESC_SIZE) {
      const usb_intf_desc_t* interfaceDesc = reinterpret_cast<const usb_intf_desc_t*>(descriptor);
      currentIsHid = interfaceDesc->bInterfaceClass == USB_CLASS_HID_LOCAL;
      currentInterface = {};
      if (currentIsHid) {
        currentInterface.interfaceNumber = interfaceDesc->bInterfaceNumber;
        currentInterface.alternateSetting = interfaceDesc->bAlternateSetting;
      }
      continue;
    }

    if (!currentIsHid ||
        descriptor->bDescriptorType != USB_B_DESCRIPTOR_TYPE_ENDPOINT ||
        descriptor->bLength < USB_EP_DESC_SIZE) {
      continue;
    }

    const usb_ep_desc_t* endpointDesc = reinterpret_cast<const usb_ep_desc_t*>(descriptor);
    if (!endpointIsSupportedHidOut(endpointDesc)) {
      continue;
    }

    currentInterface.found = true;
    currentInterface.endpointAddress = endpointDesc->bEndpointAddress;
    currentInterface.endpointAttributes = endpointDesc->bmAttributes;
    currentInterface.endpointMaxPacketSize = endpointMaxPacketSize(endpointDesc);
    *out = currentInterface;
    return true;
  }

  return out->found;
}

void resetTrackedDevice(TrackedM32OledDevice* device) {
  if (device == nullptr) {
    return;
  }
  *device = {};
}

void submitOledTransfer(TrackedM32OledDevice* device) {
  if (device == nullptr || device->handle == nullptr || device->transfer == nullptr ||
      !device->interfaceClaimed || device->transferInFlight || !device->hasCurrentFrame) {
    return;
  }

  const uint16_t page = device->currentSegment == 0 ? 0 : 2;
  buildOledRegionReport(device->transfer->data_buffer, device->currentFrame, page);
  device->transfer->device_handle = device->handle;
  device->transfer->bEndpointAddress = device->hid.endpointAddress;
  device->transfer->num_bytes = M32_OLED_REPORT_BYTES;
  device->transfer->timeout_ms = 0;

  const esp_err_t err = usb_host_transfer_submit(device->transfer);
  if (err == ESP_OK || err == ESP_ERR_NOT_FINISHED) {
    device->transferInFlight = true;
    publishTransferActive(true, ESP_OK);
    return;
  }

  ESP_LOGW(TAG, "OLED OUT submit failed: %s", esp_err_to_name(err));
  addDiagnosticLog("W", TAG, "OLED OUT submit failed: %s", esp_err_to_name(err));
  publishTransferActive(false, err);
  if (err == ESP_ERR_INVALID_STATE || err == ESP_ERR_NOT_FOUND) {
    device->actions |= M32OledActionClose;
    gHasPendingActions = true;
  }
}

void cleanupDevice(TrackedM32OledDevice* device) {
  if (device == nullptr) {
    return;
  }

  const bool wasConnected = device->connected;
  const uint8_t address = device->address;

  if (device->transferInFlight) {
    device->actions |= M32OledActionClose;
    gHasPendingActions = true;
    return;
  }

  if (device->interfaceClaimed && device->handle != nullptr) {
    const esp_err_t releaseErr = usb_host_interface_release(gClientHandle,
                                                            device->handle,
                                                            device->hid.interfaceNumber);
    if (releaseErr != ESP_OK) {
      ESP_LOGW(TAG, "OLED interface release failed: %s", esp_err_to_name(releaseErr));
      addDiagnosticLog("W", TAG, "OLED interface release failed: %s", esp_err_to_name(releaseErr));
      publishError("release_error", releaseErr);
    }
    device->interfaceClaimed = false;
  }

  if (device->transfer != nullptr) {
    const esp_err_t freeErr = usb_host_transfer_free(device->transfer);
    if (freeErr != ESP_OK) {
      ESP_LOGW(TAG, "OLED transfer free failed: %s", esp_err_to_name(freeErr));
      addDiagnosticLog("W", TAG, "OLED transfer free failed: %s", esp_err_to_name(freeErr));
      publishError("free_error", freeErr);
    }
    device->transfer = nullptr;
  }

  if (device->handle != nullptr) {
    const esp_err_t closeErr = usb_host_device_close(gClientHandle, device->handle);
    if (closeErr != ESP_OK) {
      ESP_LOGW(TAG, "OLED device close failed: %s", esp_err_to_name(closeErr));
      addDiagnosticLog("W", TAG, "OLED device close failed: %s", esp_err_to_name(closeErr));
      publishError("close_error", closeErr);
    }
    device->handle = nullptr;
  }

  if (wasConnected) {
    ESP_LOGI(TAG, "Komplete M32 OLED disconnected addr=%u", static_cast<unsigned int>(address));
    addDiagnosticLog("I", TAG, "M32 OLED disconnected addr=%u", address);
    publishDisconnected(address);
  }

  resetTrackedDevice(device);
}

void queueSplashFrame() {
  M32OledFramebuffer frame;
  frame.clear(false);
  frame.drawText(0, 0, "POCKETSYNTH", 1);
  frame.drawText(0, 9, "M32 OLED READY", 1);
  frame.drawText(0, 18, "USB HID OK", 1);
  frame.drawWaveIcon(86, 5, 38, 20);
  sendM32OledFrame(frame, 0);
}

void openDevice(TrackedM32OledDevice* device) {
  if (device == nullptr || device->address == 0 || gClientHandle == nullptr) {
    return;
  }

  esp_err_t err = usb_host_device_open(gClientHandle, device->address, &device->handle);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "USB device %u open for OLED failed: %s", static_cast<unsigned int>(device->address), esp_err_to_name(err));
    addDiagnosticLog("W", TAG, "device %u OLED open failed: %s", device->address, esp_err_to_name(err));
    publishError("open_error", err);
    resetTrackedDevice(device);
    return;
  }

  const usb_device_desc_t* deviceDesc = nullptr;
  err = usb_host_get_device_descriptor(device->handle, &deviceDesc);
  if (err != ESP_OK || deviceDesc == nullptr) {
    ESP_LOGW(TAG, "USB device descriptor for OLED failed: %s", esp_err_to_name(err));
    addDiagnosticLog("W", TAG, "OLED descriptor failed: %s", esp_err_to_name(err));
    publishError("descriptor_error", err != ESP_OK ? err : ESP_ERR_INVALID_RESPONSE);
    cleanupDevice(device);
    return;
  }
  device->vid = deviceDesc->idVendor;
  device->pid = deviceDesc->idProduct;

  if (device->vid != NATIVE_INSTRUMENTS_VID || device->pid != KOMPLETE_KONTROL_M32_PID) {
    ESP_LOGI(TAG,
             "USB device %u is not Komplete M32 vid=0x%04x pid=0x%04x",
             static_cast<unsigned int>(device->address),
             static_cast<unsigned int>(device->vid),
             static_cast<unsigned int>(device->pid));
    cleanupDevice(device);
    return;
  }

  const usb_config_desc_t* configDesc = nullptr;
  err = usb_host_get_active_config_descriptor(device->handle, &configDesc);
  if (err != ESP_OK || configDesc == nullptr) {
    ESP_LOGW(TAG, "USB active config for OLED failed: %s", esp_err_to_name(err));
    addDiagnosticLog("W", TAG, "OLED active config failed: %s", esp_err_to_name(err));
    publishError("config_error", err != ESP_OK ? err : ESP_ERR_INVALID_RESPONSE);
    cleanupDevice(device);
    return;
  }

  if (!parseM32HidInterface(configDesc, &device->hid)) {
    ESP_LOGW(TAG, "Komplete M32 HID OUT interface not found");
    addDiagnosticLog("W", TAG, "M32 HID OUT not found");
    publishError("hid_not_found", ESP_ERR_NOT_FOUND);
    cleanupDevice(device);
    return;
  }

  err = usb_host_interface_claim(gClientHandle,
                                 device->handle,
                                 device->hid.interfaceNumber,
                                 device->hid.alternateSetting);
  if (err != ESP_OK) {
    ESP_LOGW(TAG,
             "OLED HID interface claim failed intf=%u: %s",
             static_cast<unsigned int>(device->hid.interfaceNumber),
             esp_err_to_name(err));
    addDiagnosticLog("W", TAG, "OLED interface claim failed: %s", esp_err_to_name(err));
    publishError("claim_error", err);
    cleanupDevice(device);
    return;
  }
  device->interfaceClaimed = true;

  err = usb_host_transfer_alloc(M32_OLED_REPORT_BYTES, 0, &device->transfer);
  if (err != ESP_OK || device->transfer == nullptr) {
    ESP_LOGW(TAG, "OLED transfer alloc failed: %s", esp_err_to_name(err));
    addDiagnosticLog("W", TAG, "OLED transfer alloc failed: %s", esp_err_to_name(err));
    publishError("alloc_error", err != ESP_OK ? err : ESP_ERR_NO_MEM);
    cleanupDevice(device);
    return;
  }

  device->connected = true;
  device->transfer->callback = [](usb_transfer_t* transfer) {
    if (transfer == nullptr || transfer->context == nullptr) {
      return;
    }

    TrackedM32OledDevice* oledDevice = static_cast<TrackedM32OledDevice*>(transfer->context);
    oledDevice->transferInFlight = false;
    publishTransferActive(false, ESP_OK);

    if (transfer->status == USB_TRANSFER_STATUS_COMPLETED) {
      if (oledDevice->currentSegment == 0) {
        oledDevice->currentSegment = 1;
        oledDevice->actions |= M32OledActionSubmitTransfer;
        gHasPendingActions = true;
        return;
      }
      oledDevice->hasCurrentFrame = false;
      publishFrameSent();
      return;
    }

    ESP_LOGW(TAG, "OLED OUT transfer status=%s", transferStatusToString(transfer->status));
    addDiagnosticLog("W", TAG, "OLED OUT status=%s", transferStatusToString(transfer->status));

    if (transfer->status == USB_TRANSFER_STATUS_NO_DEVICE ||
        transfer->status == USB_TRANSFER_STATUS_CANCELED) {
      oledDevice->actions |= M32OledActionClose;
    } else if (transfer->status == USB_TRANSFER_STATUS_STALL) {
      oledDevice->actions |= M32OledActionClearEndpoint | M32OledActionSubmitTransfer;
    } else {
      oledDevice->actions |= M32OledActionSubmitTransfer;
    }
    gHasPendingActions = true;
  };
  device->transfer->context = device;

  ESP_LOGI(TAG,
           "Komplete M32 OLED HID OUT addr=%u intf=%u ep=0x%02x mps=%u",
           static_cast<unsigned int>(device->address),
           static_cast<unsigned int>(device->hid.interfaceNumber),
           static_cast<unsigned int>(device->hid.endpointAddress),
           static_cast<unsigned int>(device->hid.endpointMaxPacketSize));
  addDiagnosticLog("I",
                   TAG,
                   "M32 OLED addr=%u intf=%u ep=0x%02x",
                   device->address,
                   device->hid.interfaceNumber,
                   device->hid.endpointAddress);
  publishConnected(*device);
  queueSplashFrame();
}

void clearEndpoint(TrackedM32OledDevice* device) {
  if (device == nullptr || device->handle == nullptr || !device->interfaceClaimed) {
    return;
  }

  const esp_err_t err = usb_host_endpoint_clear(device->handle, device->hid.endpointAddress);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "OLED endpoint clear failed: %s", esp_err_to_name(err));
    addDiagnosticLog("W", TAG, "OLED endpoint clear failed: %s", esp_err_to_name(err));
    publishError("endpoint_clear_error", err);
    if (err == ESP_ERR_INVALID_STATE || err == ESP_ERR_NOT_FOUND) {
      device->actions |= M32OledActionClose;
      gHasPendingActions = true;
    }
  }
}

void maybeStartNextFrame(TrackedM32OledDevice* device) {
  if (device == nullptr || !device->connected || !device->interfaceClaimed ||
      device->transferInFlight || device->hasCurrentFrame || gFrameQueue == nullptr) {
    return;
  }

  const BaseType_t received = xQueueReceive(gFrameQueue, &device->currentFrame, 0);
  publishQueueState(uxQueueMessagesWaiting(gFrameQueue) > 0);
  if (received != pdTRUE) {
    return;
  }

  device->hasCurrentFrame = true;
  device->currentSegment = 0;
  submitOledTransfer(device);
}

void processPendingActions() {
  gHasPendingActions = false;
  const uint8_t actions = gDevice.actions;
  gDevice.actions = 0;

  if ((actions & M32OledActionClose) != 0) {
    cleanupDevice(&gDevice);
    return;
  }
  if ((actions & M32OledActionOpen) != 0) {
    openDevice(&gDevice);
  }
  if ((actions & M32OledActionClearEndpoint) != 0) {
    clearEndpoint(&gDevice);
  }
  if ((actions & M32OledActionSubmitTransfer) != 0) {
    submitOledTransfer(&gDevice);
  }
}

void clientEventCallback(const usb_host_client_event_msg_t* eventMsg, void*) {
  if (eventMsg == nullptr) {
    return;
  }

  switch (eventMsg->event) {
    case USB_HOST_CLIENT_EVENT_NEW_DEV:
      if (gDevice.handle != nullptr) {
        gDevice.actions |= M32OledActionClose;
      }
      gDevice.address = eventMsg->new_dev.address;
      gDevice.actions |= M32OledActionOpen;
      gHasPendingActions = true;
      break;
    case USB_HOST_CLIENT_EVENT_DEV_GONE:
      if (gDevice.handle == eventMsg->dev_gone.dev_hdl) {
        gDevice.actions = M32OledActionClose;
        gHasPendingActions = true;
      }
      break;
    default:
      break;
  }
}

void m32OledHostTask(void*) {
  usb_host_client_config_t clientConfig = {};
  clientConfig.is_synchronous = false;
  clientConfig.max_num_event_msg = 8;
  clientConfig.async.client_event_callback = clientEventCallback;
  clientConfig.async.callback_arg = nullptr;

  esp_err_t err = usb_host_client_register(&clientConfig, &gClientHandle);
  publishClientRegistered(err);

  if (err != ESP_OK) {
    ESP_LOGE(TAG, "M32 OLED client register failed: %s", esp_err_to_name(err));
    addDiagnosticLog("E", TAG, "M32 OLED client register failed: %s", esp_err_to_name(err));
    vTaskDelete(nullptr);
    return;
  }

  ESP_LOGI(TAG, "M32 OLED host client ready");
  addDiagnosticLog("I", TAG, "M32 OLED client ready");

  for (;;) {
    if (gHasPendingActions) {
      processPendingActions();
      continue;
    }

    maybeStartNextFrame(&gDevice);

    err = usb_host_client_handle_events(gClientHandle, pdMS_TO_TICKS(25));
    if (err == ESP_OK || err == ESP_ERR_TIMEOUT) {
      continue;
    }

    ESP_LOGW(TAG, "M32 OLED client event handling failed: %s", esp_err_to_name(err));
    addDiagnosticLog("W", TAG, "M32 OLED client events failed: %s", esp_err_to_name(err));
    publishError("client_error", err);
    vTaskDelay(pdMS_TO_TICKS(250));
  }
}
#endif

}  // namespace

M32OledFramebuffer::M32OledFramebuffer() {
  clear(false);
}

void M32OledFramebuffer::clear(bool on) {
  memset(bits_, on ? 0xff : 0x00, sizeof(bits_));
}

void M32OledFramebuffer::invert() {
  for (size_t i = 0; i < sizeof(bits_); ++i) {
    bits_[i] = static_cast<uint8_t>(~bits_[i]);
  }
}

void M32OledFramebuffer::setPixel(int x, int y, bool on) {
  if (x < 0 || x >= M32_OLED_WIDTH || y < 0 || y >= M32_OLED_HEIGHT) {
    return;
  }
  const size_t bitIndex = static_cast<size_t>(y * M32_OLED_WIDTH + x);
  const uint8_t mask = static_cast<uint8_t>(0x80U >> (bitIndex & 7U));
  if (on) {
    bits_[bitIndex / 8U] |= mask;
  } else {
    bits_[bitIndex / 8U] &= static_cast<uint8_t>(~mask);
  }
}

bool M32OledFramebuffer::getPixel(int x, int y) const {
  if (x < 0 || x >= M32_OLED_WIDTH || y < 0 || y >= M32_OLED_HEIGHT) {
    return false;
  }
  const size_t bitIndex = static_cast<size_t>(y * M32_OLED_WIDTH + x);
  const uint8_t mask = static_cast<uint8_t>(0x80U >> (bitIndex & 7U));
  return (bits_[bitIndex / 8U] & mask) != 0;
}

void M32OledFramebuffer::drawHLine(int x, int y, int w, bool on) {
  if (w < 0) {
    x += w;
    w = -w;
  }
  for (int i = 0; i < w; ++i) {
    setPixel(x + i, y, on);
  }
}

void M32OledFramebuffer::drawVLine(int x, int y, int h, bool on) {
  if (h < 0) {
    y += h;
    h = -h;
  }
  for (int i = 0; i < h; ++i) {
    setPixel(x, y + i, on);
  }
}

void M32OledFramebuffer::drawLine(int x0, int y0, int x1, int y1, bool on) {
  const int dx = abs(x1 - x0);
  const int sx = x0 < x1 ? 1 : -1;
  const int dy = -abs(y1 - y0);
  const int sy = y0 < y1 ? 1 : -1;
  int err = dx + dy;

  for (;;) {
    setPixel(x0, y0, on);
    if (x0 == x1 && y0 == y1) {
      break;
    }
    const int e2 = 2 * err;
    if (e2 >= dy) {
      err += dy;
      x0 += sx;
    }
    if (e2 <= dx) {
      err += dx;
      y0 += sy;
    }
  }
}

void M32OledFramebuffer::drawRect(int x, int y, int w, int h, bool on) {
  if (w <= 0 || h <= 0) {
    return;
  }
  drawHLine(x, y, w, on);
  drawHLine(x, y + h - 1, w, on);
  drawVLine(x, y, h, on);
  drawVLine(x + w - 1, y, h, on);
}

void M32OledFramebuffer::fillRect(int x, int y, int w, int h, bool on) {
  if (w <= 0 || h <= 0) {
    return;
  }
  for (int yy = 0; yy < h; ++yy) {
    drawHLine(x, y + yy, w, on);
  }
}

void M32OledFramebuffer::drawChar(int x, int y, char c, uint8_t scale, bool on) {
  if (scale == 0) {
    scale = 1;
  }

  uint8_t rows[7] = {};
  glyphRows(c, rows);
  for (int row = 0; row < 7; ++row) {
    for (int col = 0; col < 5; ++col) {
      if ((rows[row] & (1U << (4 - col))) == 0) {
        continue;
      }
      if (scale == 1) {
        setPixel(x + col, y + row, on);
      } else {
        fillRect(x + col * scale, y + row * scale, scale, scale, on);
      }
    }
  }
}

int M32OledFramebuffer::drawText(int x, int y, const char* text, uint8_t scale, bool on) {
  if (text == nullptr) {
    return x;
  }
  if (scale == 0) {
    scale = 1;
  }

  const int startX = x;
  const int advance = 6 * static_cast<int>(scale);
  const int lineAdvance = 8 * static_cast<int>(scale);
  while (*text != '\0') {
    const char c = *text++;
    if (c == '\n') {
      x = startX;
      y += lineAdvance;
      continue;
    }
    drawChar(x, y, c, scale, on);
    x += advance;
  }
  return x;
}

void M32OledFramebuffer::drawBar(int x, int y, int w, int h, float normalized, bool border, bool on) {
  if (w <= 0 || h <= 0) {
    return;
  }
  if (normalized < 0.0f) {
    normalized = 0.0f;
  }
  if (normalized > 1.0f) {
    normalized = 1.0f;
  }

  if (border) {
    drawRect(x, y, w, h, on);
    x += 1;
    y += 1;
    w -= 2;
    h -= 2;
  }
  const int fillW = static_cast<int>((static_cast<float>(w) * normalized) + 0.5f);
  if (fillW > 0 && h > 0) {
    fillRect(x, y, fillW, h, on);
  }
}

void M32OledFramebuffer::drawWaveIcon(int x, int y, int w, int h, bool on) {
  if (w <= 1 || h <= 1) {
    return;
  }

  constexpr int8_t wave[17] = {0, 2, 5, 7, 7, 5, 2, 0, -2, -5, -7, -7, -5, -2, 0, 2, 5};
  int prevX = x;
  int prevY = y + h / 2 - (wave[0] * h) / 18;
  for (int i = 1; i < 17; ++i) {
    const int xx = x + (i * (w - 1)) / 16;
    const int yy = y + h / 2 - (wave[i] * h) / 18;
    drawLine(prevX, prevY, xx, yy, on);
    prevX = xx;
    prevY = yy;
  }
}

void M32OledFramebuffer::drawWaveform(int x, int y, int w, int h, const int8_t* samples, size_t sampleCount, bool on) {
  if (samples == nullptr || sampleCount == 0 || w <= 1 || h <= 1) {
    return;
  }

  int prevX = x;
  int prevY = y + h / 2 - (static_cast<int>(samples[0]) * (h - 1)) / 254;
  for (int i = 1; i < w; ++i) {
    const size_t index = static_cast<size_t>((static_cast<uint64_t>(i) * (sampleCount - 1U)) /
                                             static_cast<uint64_t>(w - 1));
    const int xx = x + i;
    const int yy = y + h / 2 - (static_cast<int>(samples[index]) * (h - 1)) / 254;
    drawLine(prevX, prevY, xx, yy, on);
    prevX = xx;
    prevY = yy;
  }
}

bool isM32OledBuildEnabled() {
#if POCKETSYNTH_ENABLE_M32_OLED
  return true;
#else
  return false;
#endif
}

esp_err_t initializeM32Oled() {
  initializeStatusDefaults();

#if POCKETSYNTH_ENABLE_M32_OLED
  if (gInitAttempted) {
    return ESP_ERR_INVALID_STATE;
  }
  gInitAttempted = true;
  publishStarted();

  if (gFrameQueue == nullptr) {
    gFrameQueue = xQueueCreate(M32_OLED_QUEUE_DEPTH, sizeof(M32OledQueuedFrame));
    if (gFrameQueue == nullptr) {
      publishError("queue_error", ESP_ERR_NO_MEM);
      return ESP_ERR_NO_MEM;
    }
  }

  const esp_err_t hostErr = initializeUsbHostRuntime();
  if (hostErr != ESP_OK) {
    ESP_LOGE(TAG, "USB Host runtime init for M32 OLED failed: %s", esp_err_to_name(hostErr));
    addDiagnosticLog("E", TAG, "M32 OLED host runtime failed: %s", esp_err_to_name(hostErr));
    publishError("host_error", hostErr);
    return hostErr;
  }

  const BaseType_t created = xTaskCreatePinnedToCore(m32OledHostTask,
                                                     "M32OledHost",
                                                     USB_M32_OLED_TASK_STACK,
                                                     nullptr,
                                                     USB_M32_OLED_TASK_PRIORITY,
                                                     nullptr,
                                                     0);
  if (created != pdTRUE) {
    publishError("task_error", ESP_ERR_NO_MEM);
    return ESP_ERR_NO_MEM;
  }
  return ESP_OK;
#else
  return ESP_ERR_NOT_SUPPORTED;
#endif
}

esp_err_t sendM32OledFrame(const M32OledFramebuffer& frame, TickType_t timeoutTicks) {
#if POCKETSYNTH_ENABLE_M32_OLED
  if (gFrameQueue == nullptr) {
    return ESP_ERR_INVALID_STATE;
  }

  M32OledQueuedFrame queued = {};
  memcpy(queued.bits, frame.data(), sizeof(queued.bits));

  BaseType_t sent = xQueueSend(gFrameQueue, &queued, timeoutTicks);
  if (sent != pdTRUE) {
    M32OledQueuedFrame dropped = {};
    (void)xQueueReceive(gFrameQueue, &dropped, 0);
    sent = xQueueSend(gFrameQueue, &queued, 0);
  }

  publishQueueState(uxQueueMessagesWaiting(gFrameQueue) > 0);
  return sent == pdTRUE ? ESP_OK : ESP_ERR_TIMEOUT;
#else
  (void)frame;
  (void)timeoutTicks;
  return ESP_ERR_NOT_SUPPORTED;
#endif
}

esp_err_t sendM32OledText(const char* line1, const char* line2, const char* line3, TickType_t timeoutTicks) {
  M32OledFramebuffer frame;
  frame.clear(false);
  frame.drawText(0, 0, line1 != nullptr ? line1 : "", 1);
  if (line2 != nullptr) {
    frame.drawText(0, 10, line2, 1);
  }
  if (line3 != nullptr) {
    frame.drawText(0, 20, line3, 1);
  }
  return sendM32OledFrame(frame, timeoutTicks);
}

esp_err_t sendM32OledParameter(const char* name,
                               const char* value,
                               float normalizedValue,
                               TickType_t timeoutTicks) {
  M32OledFramebuffer frame;
  frame.clear(false);
  frame.drawText(0, 0, "POCKETSYNTH", 1);
  frame.drawText(0, 10, name != nullptr ? name : "PARAM", 1);
  frame.drawText(0, 20, value != nullptr ? value : "", 1);
  frame.drawBar(70, 22, 56, 8, normalizedValue, true);
  return sendM32OledFrame(frame, timeoutTicks);
}

void setM32OledOutputInverted(bool inverted) {
  portENTER_CRITICAL(&gM32OledMux);
  gOutputInverted = inverted;
  gStatus.outputInverted = inverted;
  portEXIT_CRITICAL(&gM32OledMux);
}

bool isM32OledOutputInverted() {
  bool inverted = true;
  portENTER_CRITICAL(&gM32OledMux);
  inverted = gOutputInverted;
  portEXIT_CRITICAL(&gM32OledMux);
  return inverted;
}

M32OledStatus getM32OledStatus() {
  M32OledStatus snapshot = {};
  portENTER_CRITICAL(&gM32OledMux);
  snapshot = gStatus;
  portEXIT_CRITICAL(&gM32OledMux);
  return snapshot;
}

size_t writeM32OledStatusJson(char* out, size_t outSize) {
  if (out == nullptr || outSize == 0) {
    return 0;
  }

  M32OledStatus snapshot = getM32OledStatus();
#if POCKETSYNTH_ENABLE_M32_OLED
  if (!snapshot.enabled) {
    snapshot.enabled = true;
    snapshot.lastError = ESP_ERR_INVALID_STATE;
    copyString(snapshot.lastEvent, sizeof(snapshot.lastEvent), "idle");
  }
#endif

  JsonWriter writer = {out, outSize, 0, false};
  appendStatusJson(&writer, snapshot);
  if (writer.truncated && outSize > 0) {
    out[outSize - 1] = '\0';
  }
  return writer.used;
}

}  // namespace pocketsynth
