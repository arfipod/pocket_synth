#include "usb_host_diag.h"

#include "boot_diagnostics.h"
#include "synth_config.h"

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

#ifndef POCKETSYNTH_ENABLE_USB_HOST_DIAG
#define POCKETSYNTH_ENABLE_USB_HOST_DIAG 0
#endif

#if POCKETSYNTH_ENABLE_USB_HOST_DIAG
#include "esp_bit_defs.h"
#include "esp_idf_version.h"
#include "sdkconfig.h"
#include "usb/usb_host.h"
#endif

namespace pocketsynth {
namespace {

constexpr const char* TAG = "usb_host_diag";

constexpr size_t USB_DIAG_MAX_DEVICES = 8;
constexpr size_t USB_DIAG_MAX_CONFIGS = 4;
constexpr size_t USB_DIAG_MAX_INTERFACES = 10;
constexpr size_t USB_DIAG_MAX_ENDPOINTS_PER_INTERFACE = 4;
constexpr size_t USB_DIAG_STRING_LENGTH = 48;

struct UsbDiagEndpoint {
  uint8_t address;
  uint8_t attributes;
  uint16_t maxPacketSize;
  uint8_t interval;
};

struct UsbDiagInterface {
  uint8_t number;
  uint8_t alternateSetting;
  uint8_t endpointCount;
  uint8_t classCode;
  uint8_t subclassCode;
  uint8_t protocolCode;
  UsbDiagEndpoint endpoints[USB_DIAG_MAX_ENDPOINTS_PER_INTERFACE];
};

struct UsbDiagConfiguration {
  bool valid;
  bool active;
  bool truncated;
  uint8_t value;
  uint8_t interfaceCount;
  uint8_t attributes;
  uint8_t maxPowerUnits;
  uint16_t totalLength;
  UsbDiagInterface interfaces[USB_DIAG_MAX_INTERFACES];
};

struct UsbDiagDevice {
  bool known;
  bool connected;
  bool descriptorTruncated;
  uint8_t address;
  uint8_t parentAddress;
  uint8_t parentPort;
  uint8_t speed;
  uint8_t deviceClass;
  uint8_t deviceSubclass;
  uint8_t deviceProtocol;
  uint8_t configurationCount;
  uint8_t descriptorConfigurationCount;
  uint16_t vid;
  uint16_t pid;
  uint16_t bcdUsb;
  uint16_t bcdDevice;
  esp_err_t lastError;
  uint32_t connectSequence;
  uint32_t disconnectSequence;
  char manufacturer[USB_DIAG_STRING_LENGTH];
  char product[USB_DIAG_STRING_LENGTH];
  char serial[USB_DIAG_STRING_LENGTH];
  UsbDiagConfiguration configurations[USB_DIAG_MAX_CONFIGS];
};

struct UsbDiagSnapshot {
  bool enabled;
  bool started;
  bool hostInstalled;
  bool clientRegistered;
  bool hubsSupported;
  esp_err_t lastError;
  uint32_t eventSequence;
  uint32_t connectCount;
  uint32_t disconnectCount;
  uint32_t lastEventMs;
  char lastEvent[16];
  UsbDiagDevice devices[USB_DIAG_MAX_DEVICES];
};

struct JsonWriter {
  char* out;
  size_t outSize;
  size_t used;
  bool truncated;
};

portMUX_TYPE gUsbDiagMux = portMUX_INITIALIZER_UNLOCKED;
UsbDiagSnapshot gSnapshot = {};
#if POCKETSYNTH_ENABLE_USB_HOST_DIAG
bool gInitAttempted = false;
esp_err_t gHostInstallResult = ESP_ERR_INVALID_STATE;
#endif

void copyString(char* out, size_t outSize, const char* in) {
  if (out == nullptr || outSize == 0) {
    return;
  }
  snprintf(out, outSize, "%s", in != nullptr ? in : "");
}

#if POCKETSYNTH_ENABLE_USB_HOST_DIAG
void setLastEvent(const char* eventName, esp_err_t err) {
  gSnapshot.lastError = err;
  gSnapshot.lastEventMs = static_cast<uint32_t>(esp_timer_get_time() / 1000ULL);
  copyString(gSnapshot.lastEvent, sizeof(gSnapshot.lastEvent), eventName);
}
#endif

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

const char* speedToString(uint8_t speed) {
  switch (speed) {
    case 0:
      return "low";
    case 1:
      return "full";
    case 2:
      return "high";
    default:
      return "unknown";
  }
}

const char* endpointDirection(uint8_t address) {
  return (address & 0x80U) != 0 ? "in" : "out";
}

const char* endpointTransferType(uint8_t attributes) {
  switch (attributes & 0x03U) {
    case 0:
      return "control";
    case 1:
      return "isochronous";
    case 2:
      return "bulk";
    case 3:
      return "interrupt";
    default:
      return "unknown";
  }
}

uint16_t maxPowerMilliamps(uint8_t maxPowerUnits) {
  return static_cast<uint16_t>(maxPowerUnits) * 2U;
}

size_t countKnownDevices(const UsbDiagSnapshot& snapshot) {
  size_t count = 0;
  for (const auto& device : snapshot.devices) {
    if (device.known) {
      ++count;
    }
  }
  return count;
}

size_t countConnectedDevices(const UsbDiagSnapshot& snapshot) {
  size_t count = 0;
  for (const auto& device : snapshot.devices) {
    if (device.known && device.connected) {
      ++count;
    }
  }
  return count;
}

void appendEndpointJson(JsonWriter* writer, const UsbDiagEndpoint& endpoint) {
  appendFormat(writer,
               "{"
               "\"address\":%u,"
               "\"address_hex\":\"0x%02x\","
               "\"direction\":\"%s\","
               "\"attributes\":%u,"
               "\"attributes_hex\":\"0x%02x\","
               "\"transfer_type\":\"%s\","
               "\"max_packet_size\":%u,"
               "\"interval\":%u"
               "}",
               static_cast<unsigned int>(endpoint.address),
               static_cast<unsigned int>(endpoint.address),
               endpointDirection(endpoint.address),
               static_cast<unsigned int>(endpoint.attributes),
               static_cast<unsigned int>(endpoint.attributes),
               endpointTransferType(endpoint.attributes),
               static_cast<unsigned int>(endpoint.maxPacketSize),
               static_cast<unsigned int>(endpoint.interval));
}

void appendInterfaceJson(JsonWriter* writer, const UsbDiagInterface& interface) {
  appendFormat(writer,
               "{"
               "\"number\":%u,"
               "\"alternate_setting\":%u,"
               "\"class\":%u,"
               "\"class_hex\":\"0x%02x\","
               "\"subclass\":%u,"
               "\"subclass_hex\":\"0x%02x\","
               "\"protocol\":%u,"
               "\"protocol_hex\":\"0x%02x\","
               "\"endpoint_count\":%u,"
               "\"endpoints\":[",
               static_cast<unsigned int>(interface.number),
               static_cast<unsigned int>(interface.alternateSetting),
               static_cast<unsigned int>(interface.classCode),
               static_cast<unsigned int>(interface.classCode),
               static_cast<unsigned int>(interface.subclassCode),
               static_cast<unsigned int>(interface.subclassCode),
               static_cast<unsigned int>(interface.protocolCode),
               static_cast<unsigned int>(interface.protocolCode),
               static_cast<unsigned int>(interface.endpointCount));
  const size_t endpointCount = interface.endpointCount < USB_DIAG_MAX_ENDPOINTS_PER_INTERFACE
                                  ? interface.endpointCount
                                  : USB_DIAG_MAX_ENDPOINTS_PER_INTERFACE;
  for (size_t i = 0; i < endpointCount; ++i) {
    if (i != 0) {
      appendFormat(writer, ",");
    }
    appendEndpointJson(writer, interface.endpoints[i]);
  }
  appendFormat(writer, "]}");
}

void appendConfigurationJson(JsonWriter* writer, const UsbDiagConfiguration& config) {
  appendFormat(writer,
               "{"
               "\"valid\":%s,"
               "\"active\":%s,"
               "\"truncated\":%s,"
               "\"value\":%u,"
               "\"total_length\":%u,"
               "\"interface_count\":%u,"
               "\"attributes\":%u,"
               "\"attributes_hex\":\"0x%02x\","
               "\"max_power_ma\":%u,"
               "\"interfaces\":[",
               config.valid ? "true" : "false",
               config.active ? "true" : "false",
               config.truncated ? "true" : "false",
               static_cast<unsigned int>(config.value),
               static_cast<unsigned int>(config.totalLength),
               static_cast<unsigned int>(config.interfaceCount),
               static_cast<unsigned int>(config.attributes),
               static_cast<unsigned int>(config.attributes),
               static_cast<unsigned int>(maxPowerMilliamps(config.maxPowerUnits)));
  const size_t interfaceCount = config.interfaceCount < USB_DIAG_MAX_INTERFACES
                                   ? config.interfaceCount
                                   : USB_DIAG_MAX_INTERFACES;
  for (size_t i = 0; i < interfaceCount; ++i) {
    if (i != 0) {
      appendFormat(writer, ",");
    }
    appendInterfaceJson(writer, config.interfaces[i]);
  }
  appendFormat(writer, "]}");
}

void appendDeviceJson(JsonWriter* writer, const UsbDiagDevice& device) {
  appendFormat(writer,
               "{"
               "\"connected\":%s,"
               "\"address\":%u,"
               "\"parent_address\":%u,"
               "\"parent_port\":%u,"
               "\"speed\":\"%s\","
               "\"vid\":%u,"
               "\"pid\":%u,"
               "\"vid_hex\":\"0x%04x\","
               "\"pid_hex\":\"0x%04x\","
               "\"bcd_usb\":\"0x%04x\","
               "\"bcd_device\":\"0x%04x\","
               "\"class\":%u,"
               "\"class_hex\":\"0x%02x\","
               "\"subclass\":%u,"
               "\"subclass_hex\":\"0x%02x\","
               "\"protocol\":%u,"
               "\"protocol_hex\":\"0x%02x\","
               "\"descriptor_configuration_count\":%u,"
               "\"captured_configuration_count\":%u,"
               "\"descriptor_truncated\":%s,"
               "\"last_error\":\"%s\","
               "\"connect_sequence\":%lu,"
               "\"disconnect_sequence\":%lu,"
               "\"manufacturer\":",
               device.connected ? "true" : "false",
               static_cast<unsigned int>(device.address),
               static_cast<unsigned int>(device.parentAddress),
               static_cast<unsigned int>(device.parentPort),
               speedToString(device.speed),
               static_cast<unsigned int>(device.vid),
               static_cast<unsigned int>(device.pid),
               static_cast<unsigned int>(device.vid),
               static_cast<unsigned int>(device.pid),
               static_cast<unsigned int>(device.bcdUsb),
               static_cast<unsigned int>(device.bcdDevice),
               static_cast<unsigned int>(device.deviceClass),
               static_cast<unsigned int>(device.deviceClass),
               static_cast<unsigned int>(device.deviceSubclass),
               static_cast<unsigned int>(device.deviceSubclass),
               static_cast<unsigned int>(device.deviceProtocol),
               static_cast<unsigned int>(device.deviceProtocol),
               static_cast<unsigned int>(device.descriptorConfigurationCount),
               static_cast<unsigned int>(device.configurationCount),
               device.descriptorTruncated ? "true" : "false",
               esp_err_to_name(device.lastError),
               static_cast<unsigned long>(device.connectSequence),
               static_cast<unsigned long>(device.disconnectSequence));
  appendJsonString(writer, device.manufacturer);
  appendFormat(writer, ",\"product\":");
  appendJsonString(writer, device.product);
  appendFormat(writer, ",\"serial\":");
  appendJsonString(writer, device.serial);
  appendFormat(writer, ",\"configurations\":[");
  const size_t configCount = device.configurationCount < USB_DIAG_MAX_CONFIGS
                                ? device.configurationCount
                                : USB_DIAG_MAX_CONFIGS;
  for (size_t i = 0; i < configCount; ++i) {
    if (i != 0) {
      appendFormat(writer, ",");
    }
    appendConfigurationJson(writer, device.configurations[i]);
  }
  appendFormat(writer, "]}");
}

void appendSnapshotJson(JsonWriter* writer, const UsbDiagSnapshot& snapshot) {
  appendFormat(writer,
               "{"
               "\"enabled\":%s,"
               "\"started\":%s,"
               "\"host_installed\":%s,"
               "\"client_registered\":%s,"
               "\"hubs_supported\":%s,"
               "\"known_device_count\":%u,"
               "\"connected_device_count\":%u,"
               "\"connect_count\":%lu,"
               "\"disconnect_count\":%lu,"
               "\"event_sequence\":%lu,"
               "\"last_event\":",
               snapshot.enabled ? "true" : "false",
               snapshot.started ? "true" : "false",
               snapshot.hostInstalled ? "true" : "false",
               snapshot.clientRegistered ? "true" : "false",
               snapshot.hubsSupported ? "true" : "false",
               static_cast<unsigned int>(countKnownDevices(snapshot)),
               static_cast<unsigned int>(countConnectedDevices(snapshot)),
               static_cast<unsigned long>(snapshot.connectCount),
               static_cast<unsigned long>(snapshot.disconnectCount),
               static_cast<unsigned long>(snapshot.eventSequence));
  appendJsonString(writer, snapshot.lastEvent);
  appendFormat(writer,
               ",\"last_event_ms\":%lu,"
               "\"last_error\":\"%s\","
               "\"devices\":[",
               static_cast<unsigned long>(snapshot.lastEventMs),
               esp_err_to_name(snapshot.lastError));
  bool wroteDevice = false;
  for (const auto& device : snapshot.devices) {
    if (!device.known) {
      continue;
    }
    if (wroteDevice) {
      appendFormat(writer, ",");
    }
    appendDeviceJson(writer, device);
    wroteDevice = true;
  }
  appendFormat(writer, "]}");
}

#if POCKETSYNTH_ENABLE_USB_HOST_DIAG
void copyUsbStringDescriptor(char* out, size_t outSize, const usb_str_desc_t* descriptor) {
  if (out == nullptr || outSize == 0) {
    return;
  }
  out[0] = '\0';
  if (descriptor == nullptr || descriptor->bLength <= 2) {
    return;
  }

  const size_t charCount = (descriptor->bLength - 2U) / 2U;
  size_t used = 0;
  for (size_t i = 0; i < charCount && used + 1 < outSize; ++i) {
    const uint16_t code = descriptor->wData[i];
    char ch = '?';
    if (code >= 0x20U && code <= 0x7eU) {
      ch = static_cast<char>(code);
    }
    out[used++] = ch;
  }
  out[used] = '\0';
}

size_t findDeviceSlotByAddress(const UsbDiagSnapshot& snapshot, uint8_t address) {
  for (size_t i = 0; i < USB_DIAG_MAX_DEVICES; ++i) {
    if (snapshot.devices[i].known && snapshot.devices[i].address == address) {
      return i;
    }
  }
  for (size_t i = 0; i < USB_DIAG_MAX_DEVICES; ++i) {
    if (!snapshot.devices[i].known) {
      return i;
    }
  }
  return 0;
}

void parseConfigurationDescriptor(const usb_config_desc_t* configDesc,
                                  uint8_t activeConfigurationValue,
                                  UsbDiagConfiguration* out) {
  if (configDesc == nullptr || out == nullptr) {
    return;
  }

  *out = {};
  out->valid = true;
  out->active = configDesc->bConfigurationValue == activeConfigurationValue;
  out->value = configDesc->bConfigurationValue;
  out->totalLength = configDesc->wTotalLength;
  out->attributes = configDesc->bmAttributes;
  out->maxPowerUnits = configDesc->bMaxPower;

  int offset = 0;
  const usb_standard_desc_t* descriptor = reinterpret_cast<const usb_standard_desc_t*>(configDesc);
  UsbDiagInterface* currentInterface = nullptr;
  while ((descriptor = usb_parse_next_descriptor(descriptor, configDesc->wTotalLength, &offset)) != nullptr) {
    if (descriptor->bDescriptorType == USB_B_DESCRIPTOR_TYPE_INTERFACE &&
        descriptor->bLength >= sizeof(usb_intf_desc_t)) {
      const usb_intf_desc_t* interfaceDesc = reinterpret_cast<const usb_intf_desc_t*>(descriptor);
      if (out->interfaceCount < USB_DIAG_MAX_INTERFACES) {
        currentInterface = &out->interfaces[out->interfaceCount++];
        *currentInterface = {};
        currentInterface->number = interfaceDesc->bInterfaceNumber;
        currentInterface->alternateSetting = interfaceDesc->bAlternateSetting;
        currentInterface->classCode = interfaceDesc->bInterfaceClass;
        currentInterface->subclassCode = interfaceDesc->bInterfaceSubClass;
        currentInterface->protocolCode = interfaceDesc->bInterfaceProtocol;
      } else {
        currentInterface = nullptr;
        out->truncated = true;
      }
      continue;
    }

    if (descriptor->bDescriptorType == USB_B_DESCRIPTOR_TYPE_ENDPOINT &&
        descriptor->bLength >= sizeof(usb_ep_desc_t) &&
        currentInterface != nullptr) {
      const usb_ep_desc_t* endpointDesc = reinterpret_cast<const usb_ep_desc_t*>(descriptor);
      if (currentInterface->endpointCount < USB_DIAG_MAX_ENDPOINTS_PER_INTERFACE) {
        UsbDiagEndpoint& endpoint = currentInterface->endpoints[currentInterface->endpointCount++];
        endpoint.address = endpointDesc->bEndpointAddress;
        endpoint.attributes = endpointDesc->bmAttributes;
        endpoint.maxPacketSize = endpointDesc->wMaxPacketSize;
        endpoint.interval = endpointDesc->bInterval;
      } else {
        out->truncated = true;
      }
    }
  }
}

void publishDeviceDisconnect(uint8_t address) {
  portENTER_CRITICAL(&gUsbDiagMux);
  size_t slot = USB_DIAG_MAX_DEVICES;
  for (size_t i = 0; i < USB_DIAG_MAX_DEVICES; ++i) {
    if (gSnapshot.devices[i].known && gSnapshot.devices[i].address == address) {
      slot = i;
      break;
    }
  }
  const uint32_t sequence = ++gSnapshot.eventSequence;
  ++gSnapshot.disconnectCount;
  setLastEvent("disconnected", ESP_OK);
  if (slot < USB_DIAG_MAX_DEVICES) {
    gSnapshot.devices[slot].connected = false;
    gSnapshot.devices[slot].disconnectSequence = sequence;
  }
  portEXIT_CRITICAL(&gUsbDiagMux);
}

void publishDeviceError(uint8_t address, esp_err_t err) {
  portENTER_CRITICAL(&gUsbDiagMux);
  const size_t slot = findDeviceSlotByAddress(gSnapshot, address);
  UsbDiagDevice& device = gSnapshot.devices[slot];
  device.known = true;
  device.address = address;
  device.lastError = err;
  setLastEvent("error", err);
  portEXIT_CRITICAL(&gUsbDiagMux);
}

void publishDeviceSnapshot(uint8_t address,
                           const usb_device_info_t& deviceInfo,
                           const usb_device_desc_t& deviceDesc,
                           const UsbDiagConfiguration* configurations,
                           size_t configurationCount,
                           bool descriptorTruncated,
                           esp_err_t lastError) {
  UsbDiagDevice updatedDevice = {};
  updatedDevice.known = true;
  updatedDevice.connected = true;
  updatedDevice.descriptorTruncated = descriptorTruncated;
  updatedDevice.address = address;
  updatedDevice.speed = static_cast<uint8_t>(deviceInfo.speed);
  updatedDevice.parentPort = deviceInfo.parent.port_num;
  updatedDevice.deviceClass = deviceDesc.bDeviceClass;
  updatedDevice.deviceSubclass = deviceDesc.bDeviceSubClass;
  updatedDevice.deviceProtocol = deviceDesc.bDeviceProtocol;
  updatedDevice.descriptorConfigurationCount = deviceDesc.bNumConfigurations;
  updatedDevice.vid = deviceDesc.idVendor;
  updatedDevice.pid = deviceDesc.idProduct;
  updatedDevice.bcdUsb = deviceDesc.bcdUSB;
  updatedDevice.bcdDevice = deviceDesc.bcdDevice;
  updatedDevice.lastError = lastError;

  if (deviceInfo.parent.dev_hdl != nullptr) {
    usb_device_info_t parentInfo = {};
    if (usb_host_device_info(deviceInfo.parent.dev_hdl, &parentInfo) == ESP_OK) {
      updatedDevice.parentAddress = parentInfo.dev_addr;
    }
  }

  copyUsbStringDescriptor(updatedDevice.manufacturer,
                          sizeof(updatedDevice.manufacturer),
                          deviceInfo.str_desc_manufacturer);
  copyUsbStringDescriptor(updatedDevice.product, sizeof(updatedDevice.product), deviceInfo.str_desc_product);
  copyUsbStringDescriptor(updatedDevice.serial, sizeof(updatedDevice.serial), deviceInfo.str_desc_serial_num);

  updatedDevice.configurationCount = configurationCount < USB_DIAG_MAX_CONFIGS
                                         ? static_cast<uint8_t>(configurationCount)
                                         : static_cast<uint8_t>(USB_DIAG_MAX_CONFIGS);
  for (size_t i = 0; i < updatedDevice.configurationCount; ++i) {
    updatedDevice.configurations[i] = configurations[i];
  }

  portENTER_CRITICAL(&gUsbDiagMux);
  const size_t slot = findDeviceSlotByAddress(gSnapshot, address);
  updatedDevice.connectSequence = ++gSnapshot.eventSequence;
  ++gSnapshot.connectCount;
  setLastEvent("connected", lastError);
  gSnapshot.devices[slot] = updatedDevice;
  portEXIT_CRITICAL(&gUsbDiagMux);
}

struct TrackedUsbDevice {
  uint8_t address;
  usb_device_handle_t handle;
  uint8_t actions;
};

enum UsbDiagAction : uint8_t {
  UsbDiagActionOpen = 1U << 0,
  UsbDiagActionClose = 1U << 1,
};

TrackedUsbDevice gTrackedDevices[128] = {};
usb_host_client_handle_t gClientHandle = nullptr;
bool gHasPendingActions = false;

void clientEventCallback(const usb_host_client_event_msg_t* eventMsg, void*) {
  if (eventMsg == nullptr) {
    return;
  }

  switch (eventMsg->event) {
    case USB_HOST_CLIENT_EVENT_NEW_DEV: {
      const uint8_t address = eventMsg->new_dev.address;
      if (address < sizeof(gTrackedDevices) / sizeof(gTrackedDevices[0])) {
        TrackedUsbDevice& tracked = gTrackedDevices[address];
        tracked.address = address;
        tracked.actions |= UsbDiagActionOpen;
        gHasPendingActions = true;
      }
      break;
    }
    case USB_HOST_CLIENT_EVENT_DEV_GONE: {
      for (auto& tracked : gTrackedDevices) {
        if (tracked.handle == eventMsg->dev_gone.dev_hdl) {
          tracked.actions = UsbDiagActionClose;
          gHasPendingActions = true;
          break;
        }
      }
      break;
    }
    default:
      break;
  }
}

void readDeviceDescriptors(TrackedUsbDevice* tracked) {
  if (tracked == nullptr || tracked->address == 0) {
    return;
  }

  esp_err_t err = usb_host_device_open(gClientHandle, tracked->address, &tracked->handle);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "USB device %u open failed: %s", tracked->address, esp_err_to_name(err));
    addDiagnosticLog("W", TAG, "device %u open failed: %s", tracked->address, esp_err_to_name(err));
    publishDeviceError(tracked->address, err);
    return;
  }

  usb_device_info_t deviceInfo = {};
  err = usb_host_device_info(tracked->handle, &deviceInfo);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "USB device %u info failed: %s", tracked->address, esp_err_to_name(err));
    addDiagnosticLog("W", TAG, "device %u info failed: %s", tracked->address, esp_err_to_name(err));
    publishDeviceError(tracked->address, err);
    return;
  }

  const usb_device_desc_t* deviceDesc = nullptr;
  err = usb_host_get_device_descriptor(tracked->handle, &deviceDesc);
  if (err != ESP_OK || deviceDesc == nullptr) {
    ESP_LOGW(TAG, "USB device %u descriptor failed: %s", tracked->address, esp_err_to_name(err));
    addDiagnosticLog("W", TAG, "device %u descriptor failed: %s", tracked->address, esp_err_to_name(err));
    publishDeviceError(tracked->address, err != ESP_OK ? err : ESP_ERR_INVALID_RESPONSE);
    return;
  }

  UsbDiagConfiguration configurations[USB_DIAG_MAX_CONFIGS] = {};
  size_t configurationCount = 0;
  bool descriptorTruncated = deviceDesc->bNumConfigurations > USB_DIAG_MAX_CONFIGS;
  esp_err_t descriptorErr = ESP_OK;

  for (uint8_t configValue = 1;
       configValue <= deviceDesc->bNumConfigurations && configurationCount < USB_DIAG_MAX_CONFIGS;
       ++configValue) {
    const usb_config_desc_t* configDesc = nullptr;
    bool allocatedConfigDesc = false;
    err = usb_host_get_config_desc(gClientHandle, tracked->handle, configValue, &configDesc);
    if (err == ESP_OK && configDesc != nullptr) {
      allocatedConfigDesc = true;
    } else {
      if (descriptorErr == ESP_OK) {
        descriptorErr = err != ESP_OK ? err : ESP_ERR_INVALID_RESPONSE;
      }
      const usb_config_desc_t* activeConfigDesc = nullptr;
      const esp_err_t activeErr = usb_host_get_active_config_descriptor(tracked->handle, &activeConfigDesc);
      if (activeErr == ESP_OK && activeConfigDesc != nullptr && activeConfigDesc->bConfigurationValue == configValue) {
        configDesc = activeConfigDesc;
      }
    }

    if (configDesc != nullptr) {
      parseConfigurationDescriptor(configDesc, deviceInfo.bConfigurationValue, &configurations[configurationCount]);
      ++configurationCount;
    }

    if (allocatedConfigDesc) {
      const esp_err_t freeErr = usb_host_free_config_desc(configDesc);
      if (freeErr != ESP_OK && descriptorErr == ESP_OK) {
        descriptorErr = freeErr;
      }
    }
  }

  publishDeviceSnapshot(tracked->address,
                        deviceInfo,
                        *deviceDesc,
                        configurations,
                        configurationCount,
                        descriptorTruncated,
                        descriptorErr);
  ESP_LOGI(TAG,
           "USB connected addr=%u vid=0x%04x pid=0x%04x configs=%u",
           tracked->address,
           static_cast<unsigned int>(deviceDesc->idVendor),
           static_cast<unsigned int>(deviceDesc->idProduct),
           static_cast<unsigned int>(configurationCount));
  addDiagnosticLog("I",
                   TAG,
                   "connected addr=%u vid=0x%04x pid=0x%04x",
                   tracked->address,
                   static_cast<unsigned int>(deviceDesc->idVendor),
                   static_cast<unsigned int>(deviceDesc->idProduct));
}

void closeDevice(TrackedUsbDevice* tracked) {
  if (tracked == nullptr || tracked->handle == nullptr) {
    return;
  }

  const uint8_t address = tracked->address;
  publishDeviceDisconnect(address);
  ESP_LOGI(TAG, "USB disconnected addr=%u", address);
  addDiagnosticLog("I", TAG, "disconnected addr=%u", address);

  const esp_err_t err = usb_host_device_close(gClientHandle, tracked->handle);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "USB device %u close failed: %s", address, esp_err_to_name(err));
    addDiagnosticLog("W", TAG, "device %u close failed: %s", address, esp_err_to_name(err));
    publishDeviceError(address, err);
  }
  tracked->handle = nullptr;
  tracked->address = 0;
}

void processPendingActions() {
  gHasPendingActions = false;
  for (auto& tracked : gTrackedDevices) {
    const uint8_t actions = tracked.actions;
    if (actions == 0) {
      continue;
    }
    tracked.actions = 0;
    if ((actions & UsbDiagActionClose) != 0) {
      closeDevice(&tracked);
      continue;
    }
    if ((actions & UsbDiagActionOpen) != 0) {
      readDeviceDescriptors(&tracked);
    }
  }
}

void usbHostClientTask(void*) {
  usb_host_client_config_t clientConfig = {};
  clientConfig.is_synchronous = false;
  clientConfig.max_num_event_msg = 8;
  clientConfig.async.client_event_callback = clientEventCallback;
  clientConfig.async.callback_arg = nullptr;

  esp_err_t err = usb_host_client_register(&clientConfig, &gClientHandle);
  portENTER_CRITICAL(&gUsbDiagMux);
  gSnapshot.clientRegistered = err == ESP_OK;
  setLastEvent(err == ESP_OK ? "client_ready" : "client_error", err);
  portEXIT_CRITICAL(&gUsbDiagMux);

  if (err != ESP_OK) {
    ESP_LOGE(TAG, "USB Host diagnostic client register failed: %s", esp_err_to_name(err));
    addDiagnosticLog("E", TAG, "USB diag client failed: %s", esp_err_to_name(err));
    vTaskDelete(nullptr);
    return;
  }

  ESP_LOGI(TAG, "USB Host diagnostic client ready");
  addDiagnosticLog("I", TAG, "USB Host diagnostic client ready");

  for (;;) {
    if (gHasPendingActions) {
      processPendingActions();
      continue;
    }

    err = usb_host_client_handle_events(gClientHandle, pdMS_TO_TICKS(500));
    if (err == ESP_OK || err == ESP_ERR_TIMEOUT) {
      continue;
    }

    ESP_LOGW(TAG, "USB Host client event handling failed: %s", esp_err_to_name(err));
    portENTER_CRITICAL(&gUsbDiagMux);
    setLastEvent("client_error", err);
    portEXIT_CRITICAL(&gUsbDiagMux);
    vTaskDelay(pdMS_TO_TICKS(250));
  }
}

void usbHostDaemonTask(void* notifyTaskHandle) {
  ESP_LOGI(TAG, "Installing USB Host diagnostics library");

  usb_host_config_t hostConfig = {};
  hostConfig.skip_phy_setup = false;
  hostConfig.intr_flags = ESP_INTR_FLAG_LOWMED;
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0)
  hostConfig.peripheral_map = BIT0;
#endif

  const esp_err_t err = usb_host_install(&hostConfig);
  gHostInstallResult = err;
  portENTER_CRITICAL(&gUsbDiagMux);
  gSnapshot.hostInstalled = err == ESP_OK;
  setLastEvent(err == ESP_OK ? "host_ready" : "host_error", err);
  portEXIT_CRITICAL(&gUsbDiagMux);

  if (notifyTaskHandle != nullptr) {
    xTaskNotifyGive(static_cast<TaskHandle_t>(notifyTaskHandle));
  }

  if (err != ESP_OK) {
    ESP_LOGE(TAG, "USB Host install failed: %s", esp_err_to_name(err));
    addDiagnosticLog("E", TAG, "USB Host install failed: %s", esp_err_to_name(err));
    vTaskDelete(nullptr);
    return;
  }

  ESP_LOGI(TAG, "USB Host diagnostics library ready");
  addDiagnosticLog("I", TAG, "USB Host diagnostics library ready");

  for (;;) {
    uint32_t eventFlags = 0;
    const esp_err_t handleErr = usb_host_lib_handle_events(portMAX_DELAY, &eventFlags);
    if (handleErr != ESP_OK) {
      ESP_LOGW(TAG, "USB Host library event handling failed: %s", esp_err_to_name(handleErr));
      portENTER_CRITICAL(&gUsbDiagMux);
      setLastEvent("host_error", handleErr);
      portEXIT_CRITICAL(&gUsbDiagMux);
      vTaskDelay(pdMS_TO_TICKS(250));
      continue;
    }

    if ((eventFlags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) != 0) {
      ESP_LOGW(TAG, "USB Host diagnostics has no registered clients");
      addDiagnosticLog("W", TAG, "USB Host diagnostics has no clients");
    }
  }
}
#endif

void initializeSnapshotDefaults() {
  portENTER_CRITICAL(&gUsbDiagMux);
  gSnapshot = {};
  gSnapshot.enabled = isUsbHostDiagnosticsBuildEnabled();
  gSnapshot.lastError = ESP_ERR_INVALID_STATE;
#if POCKETSYNTH_ENABLE_USB_HOST_DIAG && defined(CONFIG_USB_HOST_HUBS_SUPPORTED)
  gSnapshot.hubsSupported = CONFIG_USB_HOST_HUBS_SUPPORTED;
#else
  gSnapshot.hubsSupported = false;
#endif
  copyString(gSnapshot.lastEvent, sizeof(gSnapshot.lastEvent), "idle");
  portEXIT_CRITICAL(&gUsbDiagMux);
}

}  // namespace

bool isUsbHostDiagnosticsBuildEnabled() {
#if POCKETSYNTH_ENABLE_USB_HOST_DIAG
  return true;
#else
  return false;
#endif
}

esp_err_t initializeUsbHostDiagnostics() {
  initializeSnapshotDefaults();

#if POCKETSYNTH_ENABLE_USB_HOST_DIAG
  if (gInitAttempted) {
    return ESP_ERR_INVALID_STATE;
  }
  gInitAttempted = true;

  portENTER_CRITICAL(&gUsbDiagMux);
  gSnapshot.started = true;
  setLastEvent("starting", ESP_OK);
  portEXIT_CRITICAL(&gUsbDiagMux);

  TaskHandle_t currentTask = xTaskGetCurrentTaskHandle();
  BaseType_t created = xTaskCreatePinnedToCore(usbHostDaemonTask,
                                               "UsbHostDaemon",
                                               USB_HOST_DAEMON_TASK_STACK,
                                               currentTask,
                                               USB_HOST_DAEMON_TASK_PRIORITY,
                                               nullptr,
                                               0);
  if (created != pdTRUE) {
    portENTER_CRITICAL(&gUsbDiagMux);
    setLastEvent("host_error", ESP_ERR_NO_MEM);
    portEXIT_CRITICAL(&gUsbDiagMux);
    return ESP_ERR_NO_MEM;
  }

  const uint32_t notified = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1500));
  if (notified == 0) {
    portENTER_CRITICAL(&gUsbDiagMux);
    setLastEvent("host_timeout", ESP_ERR_TIMEOUT);
    portEXIT_CRITICAL(&gUsbDiagMux);
    return ESP_ERR_TIMEOUT;
  }
  if (gHostInstallResult != ESP_OK) {
    return gHostInstallResult;
  }

  created = xTaskCreatePinnedToCore(usbHostClientTask,
                                    "UsbHostDiag",
                                    USB_HOST_CLIENT_TASK_STACK,
                                    nullptr,
                                    USB_HOST_CLIENT_TASK_PRIORITY,
                                    nullptr,
                                    0);
  if (created != pdTRUE) {
    portENTER_CRITICAL(&gUsbDiagMux);
    setLastEvent("client_error", ESP_ERR_NO_MEM);
    portEXIT_CRITICAL(&gUsbDiagMux);
    return ESP_ERR_NO_MEM;
  }
  return ESP_OK;
#else
  return ESP_ERR_NOT_SUPPORTED;
#endif
}

size_t writeUsbHostDiagnosticsJson(char* out, size_t outSize) {
  if (out == nullptr || outSize == 0) {
    return 0;
  }

  UsbDiagSnapshot snapshot = {};
  portENTER_CRITICAL(&gUsbDiagMux);
  snapshot = gSnapshot;
  portEXIT_CRITICAL(&gUsbDiagMux);

#if POCKETSYNTH_ENABLE_USB_HOST_DIAG
  if (!snapshot.enabled) {
    snapshot.enabled = true;
    snapshot.lastError = ESP_ERR_INVALID_STATE;
#if defined(CONFIG_USB_HOST_HUBS_SUPPORTED)
    snapshot.hubsSupported = CONFIG_USB_HOST_HUBS_SUPPORTED;
#endif
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
