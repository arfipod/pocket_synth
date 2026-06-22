#include "usb_midi_host.h"

#include "boot_diagnostics.h"
#include "synth_config.h"
#include "usb_host_runtime.h"

#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "freertos/task.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifndef POCKETSYNTH_ENABLE_USB_MIDI_HOST
#define POCKETSYNTH_ENABLE_USB_MIDI_HOST 0
#endif

#if POCKETSYNTH_ENABLE_USB_MIDI_HOST
#include "usb/usb_host.h"
#endif

namespace pocketsynth {
namespace {

constexpr const char* TAG = "usb_midi";
constexpr size_t USB_MIDI_MAX_DEVICES = 4;
constexpr size_t USB_MIDI_TRANSFER_BUFFER_SIZE = 64;
constexpr uint8_t USB_AUDIO_SUBCLASS_MIDI_STREAMING = 0x03;

struct UsbMidiInterfaceInfo {
  bool found;
  uint8_t interfaceNumber;
  uint8_t alternateSetting;
  uint8_t endpointAddress;
  uint8_t endpointAttributes;
  uint16_t endpointMaxPacketSize;
};

struct UsbMidiSnapshot {
  bool enabled;
  bool started;
  bool clientRegistered;
  bool deviceConnected;
  bool interfaceClaimed;
  bool transferActive;
  esp_err_t lastError;
  uint32_t connectCount;
  uint32_t disconnectCount;
  uint32_t packetCount;
  uint32_t transferCount;
  uint32_t errorCount;
  uint32_t lastEventMs;
  uint32_t lastPacketMs;
  uint8_t address;
  uint8_t interfaceNumber;
  uint8_t alternateSetting;
  uint8_t endpointAddress;
  uint16_t endpointMaxPacketSize;
  uint16_t vid;
  uint16_t pid;
  char lastEvent[24];
  uint8_t lastPacket[4];
};

struct JsonWriter {
  char* out;
  size_t outSize;
  size_t used;
  bool truncated;
};

portMUX_TYPE gUsbMidiMux = portMUX_INITIALIZER_UNLOCKED;
UsbMidiSnapshot gSnapshot = {};

void copyString(char* out, size_t outSize, const char* in) {
  if (out == nullptr || outSize == 0) {
    return;
  }
  snprintf(out, outSize, "%s", in != nullptr ? in : "");
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
      case '\\':
        appendFormat(writer, "\\\\");
        break;
      case '"':
        appendFormat(writer, "\\\"");
        break;
      case '\n':
        appendFormat(writer, "\\n");
        break;
      case '\r':
        appendFormat(writer, "\\r");
        break;
      case '\t':
        appendFormat(writer, "\\t");
        break;
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

void initializeSnapshotDefaults() {
  portENTER_CRITICAL(&gUsbMidiMux);
  gSnapshot = {};
  gSnapshot.enabled = isUsbMidiHostBuildEnabled();
  gSnapshot.lastError = ESP_ERR_INVALID_STATE;
  copyString(gSnapshot.lastEvent, sizeof(gSnapshot.lastEvent), "idle");
  portEXIT_CRITICAL(&gUsbMidiMux);
}

void appendSnapshotJson(JsonWriter* writer, const UsbMidiSnapshot& snapshot) {
  appendFormat(writer,
               "{"
               "\"enabled\":%s,"
               "\"started\":%s,"
               "\"client_registered\":%s,"
               "\"device_connected\":%s,"
               "\"interface_claimed\":%s,"
               "\"transfer_active\":%s,"
               "\"connect_count\":%lu,"
               "\"disconnect_count\":%lu,"
               "\"packet_count\":%lu,"
               "\"transfer_count\":%lu,"
               "\"error_count\":%lu,"
               "\"last_event\":",
               snapshot.enabled ? "true" : "false",
               snapshot.started ? "true" : "false",
               snapshot.clientRegistered ? "true" : "false",
               snapshot.deviceConnected ? "true" : "false",
               snapshot.interfaceClaimed ? "true" : "false",
               snapshot.transferActive ? "true" : "false",
               static_cast<unsigned long>(snapshot.connectCount),
               static_cast<unsigned long>(snapshot.disconnectCount),
               static_cast<unsigned long>(snapshot.packetCount),
               static_cast<unsigned long>(snapshot.transferCount),
               static_cast<unsigned long>(snapshot.errorCount));
  appendJsonString(writer, snapshot.lastEvent);
  appendFormat(writer,
               ",\"last_event_ms\":%lu,"
               "\"last_packet_ms\":%lu,"
               "\"last_error\":\"%s\","
               "\"address\":%u,"
               "\"vid_hex\":\"0x%04x\","
               "\"pid_hex\":\"0x%04x\","
               "\"interface\":%u,"
               "\"alternate_setting\":%u,"
               "\"endpoint_address\":\"0x%02x\","
               "\"endpoint_max_packet_size\":%u,"
               "\"last_packet\":\"%02x %02x %02x %02x\""
               "}",
               static_cast<unsigned long>(snapshot.lastEventMs),
               static_cast<unsigned long>(snapshot.lastPacketMs),
               esp_err_to_name(snapshot.lastError),
               static_cast<unsigned int>(snapshot.address),
               static_cast<unsigned int>(snapshot.vid),
               static_cast<unsigned int>(snapshot.pid),
               static_cast<unsigned int>(snapshot.interfaceNumber),
               static_cast<unsigned int>(snapshot.alternateSetting),
               static_cast<unsigned int>(snapshot.endpointAddress),
               static_cast<unsigned int>(snapshot.endpointMaxPacketSize),
               static_cast<unsigned int>(snapshot.lastPacket[0]),
               static_cast<unsigned int>(snapshot.lastPacket[1]),
               static_cast<unsigned int>(snapshot.lastPacket[2]),
               static_cast<unsigned int>(snapshot.lastPacket[3]));
}

#if POCKETSYNTH_ENABLE_USB_MIDI_HOST
void setLastEvent(UsbMidiSnapshot* snapshot, const char* eventName, esp_err_t err) {
  if (snapshot == nullptr) {
    return;
  }
  snapshot->lastError = err;
  snapshot->lastEventMs = static_cast<uint32_t>(esp_timer_get_time() / 1000ULL);
  copyString(snapshot->lastEvent, sizeof(snapshot->lastEvent), eventName);
}

struct TrackedUsbMidiDevice {
  uint8_t address;
  usb_device_handle_t handle;
  usb_transfer_t* transfer;
  UsbMidiInterfaceInfo midi;
  bool connected;
  bool interfaceClaimed;
  bool transferInFlight;
  uint16_t vid;
  uint16_t pid;
  uint8_t actions;
};

enum UsbMidiAction : uint8_t {
  UsbMidiActionOpen = 1U << 0,
  UsbMidiActionClose = 1U << 1,
  UsbMidiActionSubmitTransfer = 1U << 2,
  UsbMidiActionClearEndpoint = 1U << 3,
};

TrackedUsbMidiDevice gTrackedDevices[USB_MIDI_MAX_DEVICES] = {};
usb_host_client_handle_t gClientHandle = nullptr;
bool gInitAttempted = false;
bool gHasPendingActions = false;

const char* transferStatusToString(usb_transfer_status_t status) {
  switch (status) {
    case USB_TRANSFER_STATUS_COMPLETED:
      return "completed";
    case USB_TRANSFER_STATUS_ERROR:
      return "error";
    case USB_TRANSFER_STATUS_TIMED_OUT:
      return "timed_out";
    case USB_TRANSFER_STATUS_CANCELED:
      return "canceled";
    case USB_TRANSFER_STATUS_STALL:
      return "stall";
    case USB_TRANSFER_STATUS_OVERFLOW:
      return "overflow";
    case USB_TRANSFER_STATUS_SKIPPED:
      return "skipped";
    case USB_TRANSFER_STATUS_NO_DEVICE:
      return "no_device";
    default:
      return "unknown";
  }
}

void publishStarted() {
  portENTER_CRITICAL(&gUsbMidiMux);
  gSnapshot.started = true;
  setLastEvent(&gSnapshot, "starting", ESP_OK);
  portEXIT_CRITICAL(&gUsbMidiMux);
}

void publishClientRegistered(esp_err_t err) {
  portENTER_CRITICAL(&gUsbMidiMux);
  gSnapshot.clientRegistered = err == ESP_OK;
  setLastEvent(&gSnapshot, err == ESP_OK ? "client_ready" : "client_error", err);
  if (err != ESP_OK) {
    ++gSnapshot.errorCount;
  }
  portEXIT_CRITICAL(&gUsbMidiMux);
}

void publishError(const char* eventName, esp_err_t err) {
  portENTER_CRITICAL(&gUsbMidiMux);
  ++gSnapshot.errorCount;
  setLastEvent(&gSnapshot, eventName, err);
  portEXIT_CRITICAL(&gUsbMidiMux);
}

void publishConnected(const TrackedUsbMidiDevice& device) {
  portENTER_CRITICAL(&gUsbMidiMux);
  gSnapshot.deviceConnected = true;
  gSnapshot.interfaceClaimed = device.interfaceClaimed;
  gSnapshot.transferActive = device.transferInFlight;
  gSnapshot.address = device.address;
  gSnapshot.interfaceNumber = device.midi.interfaceNumber;
  gSnapshot.alternateSetting = device.midi.alternateSetting;
  gSnapshot.endpointAddress = device.midi.endpointAddress;
  gSnapshot.endpointMaxPacketSize = device.midi.endpointMaxPacketSize;
  gSnapshot.vid = device.vid;
  gSnapshot.pid = device.pid;
  ++gSnapshot.connectCount;
  setLastEvent(&gSnapshot, "connected", ESP_OK);
  portEXIT_CRITICAL(&gUsbMidiMux);
}

void publishDisconnected(uint8_t address) {
  portENTER_CRITICAL(&gUsbMidiMux);
  gSnapshot.deviceConnected = false;
  gSnapshot.interfaceClaimed = false;
  gSnapshot.transferActive = false;
  gSnapshot.address = address;
  ++gSnapshot.disconnectCount;
  setLastEvent(&gSnapshot, "disconnected", ESP_OK);
  portEXIT_CRITICAL(&gUsbMidiMux);
}

void publishTransferActive(bool active, esp_err_t err) {
  portENTER_CRITICAL(&gUsbMidiMux);
  gSnapshot.transferActive = active;
  if (active) {
    ++gSnapshot.transferCount;
    setLastEvent(&gSnapshot, "transfer", ESP_OK);
  } else if (err != ESP_OK) {
    ++gSnapshot.errorCount;
    setLastEvent(&gSnapshot, "transfer_error", err);
  }
  portEXIT_CRITICAL(&gUsbMidiMux);
}

void publishPacket(const uint8_t* packet) {
  uint32_t packetCount = 0;
  if (packet == nullptr) {
    return;
  }

  portENTER_CRITICAL(&gUsbMidiMux);
  ++gSnapshot.packetCount;
  packetCount = gSnapshot.packetCount;
  gSnapshot.lastPacketMs = static_cast<uint32_t>(esp_timer_get_time() / 1000ULL);
  for (size_t i = 0; i < 4; ++i) {
    gSnapshot.lastPacket[i] = packet[i];
  }
  setLastEvent(&gSnapshot, "packet", ESP_OK);
  portEXIT_CRITICAL(&gUsbMidiMux);

  ESP_LOGI(TAG,
           "USB MIDI packet #%lu: %02x %02x %02x %02x",
           static_cast<unsigned long>(packetCount),
           static_cast<unsigned int>(packet[0]),
           static_cast<unsigned int>(packet[1]),
           static_cast<unsigned int>(packet[2]),
           static_cast<unsigned int>(packet[3]));
  addDiagnosticLog("I",
                   TAG,
                   "pkt #%lu %02x %02x %02x %02x",
                   static_cast<unsigned long>(packetCount),
                   static_cast<unsigned int>(packet[0]),
                   static_cast<unsigned int>(packet[1]),
                   static_cast<unsigned int>(packet[2]),
                   static_cast<unsigned int>(packet[3]));
}

void resetTrackedDevice(TrackedUsbMidiDevice* device) {
  if (device == nullptr) {
    return;
  }
  *device = {};
}

TrackedUsbMidiDevice* findDeviceSlotByAddress(uint8_t address) {
  for (auto& device : gTrackedDevices) {
    if (device.address == address) {
      return &device;
    }
  }
  return nullptr;
}

TrackedUsbMidiDevice* findFreeDeviceSlot() {
  for (auto& device : gTrackedDevices) {
    if (device.address == 0 && device.handle == nullptr) {
      return &device;
    }
  }
  return nullptr;
}

TrackedUsbMidiDevice* findDeviceSlotByHandle(usb_device_handle_t handle) {
  for (auto& device : gTrackedDevices) {
    if (device.handle == handle) {
      return &device;
    }
  }
  return nullptr;
}

uint16_t endpointMaxPacketSize(const usb_ep_desc_t* endpointDesc) {
  if (endpointDesc == nullptr) {
    return 0;
  }
  return endpointDesc->wMaxPacketSize & USB_W_MAX_PACKET_SIZE_MPS_MASK;
}

bool endpointIsSupportedMidiIn(const usb_ep_desc_t* endpointDesc) {
  if (endpointDesc == nullptr) {
    return false;
  }

  const bool isIn = (endpointDesc->bEndpointAddress & USB_B_ENDPOINT_ADDRESS_EP_DIR_MASK) != 0;
  const uint8_t transferType = endpointDesc->bmAttributes & USB_BM_ATTRIBUTES_XFERTYPE_MASK;
  return isIn &&
         (transferType == USB_BM_ATTRIBUTES_XFER_BULK || transferType == USB_BM_ATTRIBUTES_XFER_INT);
}

bool endpointIsBulk(const UsbMidiInterfaceInfo& info) {
  return (info.endpointAttributes & USB_BM_ATTRIBUTES_XFERTYPE_MASK) == USB_BM_ATTRIBUTES_XFER_BULK;
}

bool parseMidiStreamingInterface(const usb_config_desc_t* configDesc, UsbMidiInterfaceInfo* out) {
  if (configDesc == nullptr || out == nullptr) {
    return false;
  }

  *out = {};
  bool currentIsMidiStreaming = false;
  UsbMidiInterfaceInfo currentInterface = {};

  int offset = 0;
  const usb_standard_desc_t* descriptor = reinterpret_cast<const usb_standard_desc_t*>(configDesc);
  while ((descriptor = usb_parse_next_descriptor(descriptor, configDesc->wTotalLength, &offset)) != nullptr) {
    if (descriptor->bDescriptorType == USB_B_DESCRIPTOR_TYPE_INTERFACE &&
        descriptor->bLength >= USB_INTF_DESC_SIZE) {
      const usb_intf_desc_t* interfaceDesc = reinterpret_cast<const usb_intf_desc_t*>(descriptor);
      currentIsMidiStreaming = interfaceDesc->bInterfaceClass == USB_CLASS_AUDIO &&
                               interfaceDesc->bInterfaceSubClass == USB_AUDIO_SUBCLASS_MIDI_STREAMING;
      currentInterface = {};
      if (currentIsMidiStreaming) {
        currentInterface.interfaceNumber = interfaceDesc->bInterfaceNumber;
        currentInterface.alternateSetting = interfaceDesc->bAlternateSetting;
      }
      continue;
    }

    if (!currentIsMidiStreaming ||
        descriptor->bDescriptorType != USB_B_DESCRIPTOR_TYPE_ENDPOINT ||
        descriptor->bLength < USB_EP_DESC_SIZE) {
      continue;
    }

    const usb_ep_desc_t* endpointDesc = reinterpret_cast<const usb_ep_desc_t*>(descriptor);
    if (!endpointIsSupportedMidiIn(endpointDesc)) {
      continue;
    }

    currentInterface.found = true;
    currentInterface.endpointAddress = endpointDesc->bEndpointAddress;
    currentInterface.endpointAttributes = endpointDesc->bmAttributes;
    currentInterface.endpointMaxPacketSize = endpointMaxPacketSize(endpointDesc);

    if (!out->found || endpointIsBulk(currentInterface)) {
      *out = currentInterface;
    }
    if (endpointIsBulk(currentInterface)) {
      return true;
    }
  }

  return out->found;
}

uint16_t transferSizeForEndpoint(uint16_t endpointMps) {
  if (endpointMps == 0 || endpointMps > USB_MIDI_TRANSFER_BUFFER_SIZE) {
    return 0;
  }
  return static_cast<uint16_t>((USB_MIDI_TRANSFER_BUFFER_SIZE / endpointMps) * endpointMps);
}

void logDescriptorMatch(const TrackedUsbMidiDevice& device) {
  ESP_LOGI(TAG,
           "USB MIDI IN addr=%u vid=0x%04x pid=0x%04x intf=%u alt=%u ep=0x%02x mps=%u",
           static_cast<unsigned int>(device.address),
           static_cast<unsigned int>(device.vid),
           static_cast<unsigned int>(device.pid),
           static_cast<unsigned int>(device.midi.interfaceNumber),
           static_cast<unsigned int>(device.midi.alternateSetting),
           static_cast<unsigned int>(device.midi.endpointAddress),
           static_cast<unsigned int>(device.midi.endpointMaxPacketSize));
  addDiagnosticLog("I",
                   TAG,
                   "MIDI IN addr=%u vid=0x%04x pid=0x%04x intf=%u ep=0x%02x",
                   static_cast<unsigned int>(device.address),
                   static_cast<unsigned int>(device.vid),
                   static_cast<unsigned int>(device.pid),
                   static_cast<unsigned int>(device.midi.interfaceNumber),
                   static_cast<unsigned int>(device.midi.endpointAddress));
}

void submitMidiInTransfer(TrackedUsbMidiDevice* device) {
  if (device == nullptr || device->handle == nullptr || device->transfer == nullptr ||
      !device->interfaceClaimed || device->transferInFlight) {
    return;
  }

  const uint16_t transferSize = transferSizeForEndpoint(device->midi.endpointMaxPacketSize);
  if (transferSize == 0) {
    ESP_LOGW(TAG,
             "Unsupported USB MIDI endpoint MPS %u",
             static_cast<unsigned int>(device->midi.endpointMaxPacketSize));
    addDiagnosticLog("W",
                     TAG,
                     "unsupported endpoint MPS %u",
                     static_cast<unsigned int>(device->midi.endpointMaxPacketSize));
    publishError("bad_mps", ESP_ERR_NOT_SUPPORTED);
    device->actions |= UsbMidiActionClose;
    gHasPendingActions = true;
    return;
  }

  device->transfer->device_handle = device->handle;
  device->transfer->bEndpointAddress = device->midi.endpointAddress;
  device->transfer->num_bytes = transferSize;
  device->transfer->timeout_ms = 0;

  const esp_err_t err = usb_host_transfer_submit(device->transfer);
  if (err == ESP_OK) {
    device->transferInFlight = true;
    publishTransferActive(true, ESP_OK);
    return;
  }

  if (err == ESP_ERR_NOT_FINISHED) {
    device->transferInFlight = true;
    publishTransferActive(true, ESP_OK);
    return;
  }

  ESP_LOGW(TAG, "USB MIDI IN submit failed: %s", esp_err_to_name(err));
  addDiagnosticLog("W", TAG, "IN submit failed: %s", esp_err_to_name(err));
  publishTransferActive(false, err);
  if (err == ESP_ERR_INVALID_STATE || err == ESP_ERR_NOT_FOUND) {
    device->actions |= UsbMidiActionClose;
    gHasPendingActions = true;
  }
}

void cleanupDevice(TrackedUsbMidiDevice* device) {
  if (device == nullptr) {
    return;
  }

  const bool wasConnected = device->connected;
  const uint8_t address = device->address;

  if (device->transferInFlight) {
    device->actions |= UsbMidiActionClose;
    gHasPendingActions = true;
    return;
  }

  if (device->interfaceClaimed && device->handle != nullptr) {
    const esp_err_t releaseErr = usb_host_interface_release(gClientHandle,
                                                            device->handle,
                                                            device->midi.interfaceNumber);
    if (releaseErr != ESP_OK) {
      ESP_LOGW(TAG,
               "USB MIDI interface release failed addr=%u: %s",
               static_cast<unsigned int>(address),
               esp_err_to_name(releaseErr));
      addDiagnosticLog("W", TAG, "interface release failed: %s", esp_err_to_name(releaseErr));
      publishError("release_error", releaseErr);
    }
    device->interfaceClaimed = false;
  }

  if (device->handle != nullptr) {
    const esp_err_t closeErr = usb_host_device_close(gClientHandle, device->handle);
    if (closeErr != ESP_OK) {
      ESP_LOGW(TAG,
               "USB MIDI device close failed addr=%u: %s",
               static_cast<unsigned int>(address),
               esp_err_to_name(closeErr));
      addDiagnosticLog("W", TAG, "device close failed: %s", esp_err_to_name(closeErr));
      publishError("close_error", closeErr);
    }
    device->handle = nullptr;
  }

  if (device->transfer != nullptr) {
    const esp_err_t freeErr = usb_host_transfer_free(device->transfer);
    if (freeErr != ESP_OK) {
      ESP_LOGW(TAG, "USB MIDI transfer free failed: %s", esp_err_to_name(freeErr));
      addDiagnosticLog("W", TAG, "transfer free failed: %s", esp_err_to_name(freeErr));
      publishError("free_error", freeErr);
    }
    device->transfer = nullptr;
  }

  if (wasConnected) {
    ESP_LOGI(TAG, "USB MIDI disconnected addr=%u", static_cast<unsigned int>(address));
    addDiagnosticLog("I", TAG, "disconnected addr=%u", static_cast<unsigned int>(address));
    publishDisconnected(address);
  }

  resetTrackedDevice(device);
}

void openDevice(TrackedUsbMidiDevice* device) {
  if (device == nullptr || device->address == 0 || gClientHandle == nullptr) {
    return;
  }

  esp_err_t err = usb_host_device_open(gClientHandle, device->address, &device->handle);
  if (err != ESP_OK) {
    ESP_LOGW(TAG,
             "USB device %u open for MIDI failed: %s",
             static_cast<unsigned int>(device->address),
             esp_err_to_name(err));
    addDiagnosticLog("W", TAG, "device %u MIDI open failed: %s", device->address, esp_err_to_name(err));
    publishError("open_error", err);
    resetTrackedDevice(device);
    return;
  }

  const usb_device_desc_t* deviceDesc = nullptr;
  err = usb_host_get_device_descriptor(device->handle, &deviceDesc);
  if (err != ESP_OK || deviceDesc == nullptr) {
    ESP_LOGW(TAG,
             "USB device %u descriptor for MIDI failed: %s",
             static_cast<unsigned int>(device->address),
             esp_err_to_name(err));
    addDiagnosticLog("W", TAG, "MIDI descriptor failed: %s", esp_err_to_name(err));
    publishError("descriptor_error", err != ESP_OK ? err : ESP_ERR_INVALID_RESPONSE);
    cleanupDevice(device);
    return;
  }
  device->vid = deviceDesc->idVendor;
  device->pid = deviceDesc->idProduct;

  const usb_config_desc_t* configDesc = nullptr;
  err = usb_host_get_active_config_descriptor(device->handle, &configDesc);
  if (err != ESP_OK || configDesc == nullptr) {
    ESP_LOGW(TAG,
             "USB device %u active config for MIDI failed: %s",
             static_cast<unsigned int>(device->address),
             esp_err_to_name(err));
    addDiagnosticLog("W", TAG, "MIDI active config failed: %s", esp_err_to_name(err));
    publishError("config_error", err != ESP_OK ? err : ESP_ERR_INVALID_RESPONSE);
    cleanupDevice(device);
    return;
  }

  if (!parseMidiStreamingInterface(configDesc, &device->midi)) {
    ESP_LOGI(TAG,
             "USB device %u has no class-compliant MIDI streaming IN endpoint",
             static_cast<unsigned int>(device->address));
    cleanupDevice(device);
    return;
  }

  if (transferSizeForEndpoint(device->midi.endpointMaxPacketSize) == 0) {
    ESP_LOGW(TAG,
             "USB MIDI endpoint unsupported mps=%u addr=%u",
             static_cast<unsigned int>(device->midi.endpointMaxPacketSize),
             static_cast<unsigned int>(device->address));
    addDiagnosticLog("W",
                     TAG,
                     "unsupported MIDI endpoint mps=%u",
                     static_cast<unsigned int>(device->midi.endpointMaxPacketSize));
    publishError("bad_mps", ESP_ERR_NOT_SUPPORTED);
    cleanupDevice(device);
    return;
  }

  err = usb_host_interface_claim(gClientHandle,
                                 device->handle,
                                 device->midi.interfaceNumber,
                                 device->midi.alternateSetting);
  if (err != ESP_OK) {
    ESP_LOGW(TAG,
             "USB MIDI interface claim failed addr=%u intf=%u: %s",
             static_cast<unsigned int>(device->address),
             static_cast<unsigned int>(device->midi.interfaceNumber),
             esp_err_to_name(err));
    addDiagnosticLog("W", TAG, "interface claim failed: %s", esp_err_to_name(err));
    publishError("claim_error", err);
    cleanupDevice(device);
    return;
  }
  device->interfaceClaimed = true;

  err = usb_host_transfer_alloc(USB_MIDI_TRANSFER_BUFFER_SIZE, 0, &device->transfer);
  if (err != ESP_OK || device->transfer == nullptr) {
    ESP_LOGW(TAG, "USB MIDI transfer alloc failed: %s", esp_err_to_name(err));
    addDiagnosticLog("W", TAG, "transfer alloc failed: %s", esp_err_to_name(err));
    publishError("alloc_error", err != ESP_OK ? err : ESP_ERR_NO_MEM);
    cleanupDevice(device);
    return;
  }

  device->connected = true;
  device->transfer->callback = [](usb_transfer_t* transfer) {
    if (transfer == nullptr || transfer->context == nullptr) {
      return;
    }

    TrackedUsbMidiDevice* midiDevice = static_cast<TrackedUsbMidiDevice*>(transfer->context);
    midiDevice->transferInFlight = false;
    publishTransferActive(false, ESP_OK);

    if (transfer->status == USB_TRANSFER_STATUS_COMPLETED) {
      const int bytes = transfer->actual_num_bytes;
      for (int offset = 0; offset + 3 < bytes; offset += 4) {
        publishPacket(transfer->data_buffer + offset);
      }
      if ((bytes % 4) != 0) {
        ESP_LOGW(TAG, "USB MIDI short raw packet tail: %d bytes", bytes % 4);
        addDiagnosticLog("W", TAG, "short MIDI packet tail: %d bytes", bytes % 4);
      }
      midiDevice->actions |= UsbMidiActionSubmitTransfer;
      gHasPendingActions = true;
      return;
    }

    ESP_LOGW(TAG, "USB MIDI IN transfer status=%s", transferStatusToString(transfer->status));
    addDiagnosticLog("W", TAG, "IN status=%s", transferStatusToString(transfer->status));

    if (transfer->status == USB_TRANSFER_STATUS_NO_DEVICE ||
        transfer->status == USB_TRANSFER_STATUS_CANCELED) {
      midiDevice->actions |= UsbMidiActionClose;
    } else if (transfer->status == USB_TRANSFER_STATUS_STALL) {
      midiDevice->actions |= UsbMidiActionClearEndpoint | UsbMidiActionSubmitTransfer;
    } else {
      midiDevice->actions |= UsbMidiActionSubmitTransfer;
    }
    gHasPendingActions = true;
  };
  device->transfer->context = device;

  logDescriptorMatch(*device);
  publishConnected(*device);
  device->actions |= UsbMidiActionSubmitTransfer;
  gHasPendingActions = true;
}

void clearEndpoint(TrackedUsbMidiDevice* device) {
  if (device == nullptr || device->handle == nullptr || !device->interfaceClaimed) {
    return;
  }

  const esp_err_t err = usb_host_endpoint_clear(device->handle, device->midi.endpointAddress);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "USB MIDI endpoint clear failed: %s", esp_err_to_name(err));
    addDiagnosticLog("W", TAG, "endpoint clear failed: %s", esp_err_to_name(err));
    publishError("endpoint_clear_error", err);
    if (err == ESP_ERR_INVALID_STATE || err == ESP_ERR_NOT_FOUND) {
      device->actions |= UsbMidiActionClose;
      gHasPendingActions = true;
    }
  }
}

void processPendingActions() {
  gHasPendingActions = false;
  for (auto& device : gTrackedDevices) {
    const uint8_t actions = device.actions;
    if (actions == 0) {
      continue;
    }
    device.actions = 0;

    if ((actions & UsbMidiActionClose) != 0) {
      cleanupDevice(&device);
      continue;
    }
    if ((actions & UsbMidiActionOpen) != 0) {
      openDevice(&device);
    }
    if ((actions & UsbMidiActionClearEndpoint) != 0) {
      clearEndpoint(&device);
    }
    if ((actions & UsbMidiActionSubmitTransfer) != 0) {
      submitMidiInTransfer(&device);
    }
  }
}

void clientEventCallback(const usb_host_client_event_msg_t* eventMsg, void*) {
  if (eventMsg == nullptr) {
    return;
  }

  switch (eventMsg->event) {
    case USB_HOST_CLIENT_EVENT_NEW_DEV: {
      const uint8_t address = eventMsg->new_dev.address;
      TrackedUsbMidiDevice* device = findDeviceSlotByAddress(address);
      if (device == nullptr) {
        device = findFreeDeviceSlot();
      }
      if (device == nullptr) {
        ESP_LOGW(TAG, "No USB MIDI tracking slot for device addr=%u", static_cast<unsigned int>(address));
        addDiagnosticLog("W", TAG, "no tracking slot for addr=%u", static_cast<unsigned int>(address));
        publishError("slot_error", ESP_ERR_NO_MEM);
        break;
      }
      if (device->handle != nullptr) {
        device->actions |= UsbMidiActionClose;
      }
      device->address = address;
      device->actions |= UsbMidiActionOpen;
      gHasPendingActions = true;
      break;
    }
    case USB_HOST_CLIENT_EVENT_DEV_GONE: {
      TrackedUsbMidiDevice* device = findDeviceSlotByHandle(eventMsg->dev_gone.dev_hdl);
      if (device != nullptr) {
        device->actions = UsbMidiActionClose;
        gHasPendingActions = true;
      }
      break;
    }
    default:
      break;
  }
}

void usbMidiHostTask(void*) {
  usb_host_client_config_t clientConfig = {};
  clientConfig.is_synchronous = false;
  clientConfig.max_num_event_msg = 8;
  clientConfig.async.client_event_callback = clientEventCallback;
  clientConfig.async.callback_arg = nullptr;

  esp_err_t err = usb_host_client_register(&clientConfig, &gClientHandle);
  publishClientRegistered(err);

  if (err != ESP_OK) {
    ESP_LOGE(TAG, "USB MIDI client register failed: %s", esp_err_to_name(err));
    addDiagnosticLog("E", TAG, "client register failed: %s", esp_err_to_name(err));
    vTaskDelete(nullptr);
    return;
  }

  ESP_LOGI(TAG, "USB MIDI host client ready");
  addDiagnosticLog("I", TAG, "client ready");

  for (;;) {
    if (gHasPendingActions) {
      processPendingActions();
      continue;
    }

    err = usb_host_client_handle_events(gClientHandle, pdMS_TO_TICKS(500));
    if (err == ESP_OK || err == ESP_ERR_TIMEOUT) {
      continue;
    }

    ESP_LOGW(TAG, "USB MIDI client event handling failed: %s", esp_err_to_name(err));
    addDiagnosticLog("W", TAG, "client events failed: %s", esp_err_to_name(err));
    publishError("client_error", err);
    vTaskDelay(pdMS_TO_TICKS(250));
  }
}
#endif

}  // namespace

bool isUsbMidiHostBuildEnabled() {
#if POCKETSYNTH_ENABLE_USB_MIDI_HOST
  return true;
#else
  return false;
#endif
}

esp_err_t initializeUsbMidiHost() {
  initializeSnapshotDefaults();

#if POCKETSYNTH_ENABLE_USB_MIDI_HOST
  if (gInitAttempted) {
    return ESP_ERR_INVALID_STATE;
  }
  gInitAttempted = true;
  publishStarted();

  const esp_err_t hostErr = initializeUsbHostRuntime();
  if (hostErr != ESP_OK) {
    ESP_LOGE(TAG, "USB Host runtime init for MIDI failed: %s", esp_err_to_name(hostErr));
    addDiagnosticLog("E", TAG, "host runtime failed: %s", esp_err_to_name(hostErr));
    publishError("host_error", hostErr);
    return hostErr;
  }

  const BaseType_t created = xTaskCreatePinnedToCore(usbMidiHostTask,
                                                     "UsbMidiHost",
                                                     USB_MIDI_HOST_TASK_STACK,
                                                     nullptr,
                                                     USB_MIDI_HOST_TASK_PRIORITY,
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

size_t writeUsbMidiHostStatusJson(char* out, size_t outSize) {
  if (out == nullptr || outSize == 0) {
    return 0;
  }

  UsbMidiSnapshot snapshot = {};
  portENTER_CRITICAL(&gUsbMidiMux);
  snapshot = gSnapshot;
  portEXIT_CRITICAL(&gUsbMidiMux);

#if POCKETSYNTH_ENABLE_USB_MIDI_HOST
  if (!snapshot.enabled) {
    snapshot.enabled = true;
    snapshot.lastError = ESP_ERR_INVALID_STATE;
    copyString(snapshot.lastEvent, sizeof(snapshot.lastEvent), "idle");
  }
#endif

  JsonWriter writer = {out, outSize, 0, false};
  appendSnapshotJson(&writer, snapshot);
  if (writer.truncated && outSize > 0) {
    out[outSize - 1] = '\0';
  }
  return writer.used;
}

}  // namespace pocketsynth
