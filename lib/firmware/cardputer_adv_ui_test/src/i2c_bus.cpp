#include "i2c_bus.h"

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

namespace {

constexpr i2c_port_num_t CARDPUTER_I2C_PORT = I2C_NUM_0;
constexpr gpio_num_t PIN_I2C_SDA = GPIO_NUM_8;
constexpr gpio_num_t PIN_I2C_SCL = GPIO_NUM_9;
constexpr uint32_t CARDPUTER_I2C_SPEED_HZ = 100000;
constexpr size_t MAX_I2C_DEVICES = 8;

struct I2cDeviceSlot {
  uint16_t address = 0;
  i2c_master_dev_handle_t handle = nullptr;
};

i2c_master_bus_handle_t gBusHandle = nullptr;
SemaphoreHandle_t gI2cMutex = nullptr;
I2cDeviceSlot gDeviceSlots[MAX_I2C_DEVICES] = {};

bool ensureI2cMutex() {
  if (gI2cMutex != nullptr) return true;
  gI2cMutex = xSemaphoreCreateMutex();
  return gI2cMutex != nullptr;
}

esp_err_t createI2cBus() {
  i2c_master_bus_config_t busConfig = {};
  busConfig.i2c_port = CARDPUTER_I2C_PORT;
  busConfig.sda_io_num = PIN_I2C_SDA;
  busConfig.scl_io_num = PIN_I2C_SCL;
  busConfig.clk_source = I2C_CLK_SRC_DEFAULT;
  busConfig.glitch_ignore_cnt = 7;
  busConfig.intr_priority = 0;
  busConfig.trans_queue_depth = 0;
  busConfig.flags.enable_internal_pullup = true;

  return i2c_new_master_bus(&busConfig, &gBusHandle);
}

esp_err_t deviceHandleForAddress(uint16_t address, i2c_master_dev_handle_t* out) {
  if (out == nullptr) return ESP_ERR_INVALID_ARG;

  for (auto& slot : gDeviceSlots) {
    if (slot.handle != nullptr && slot.address == address) {
      *out = slot.handle;
      return ESP_OK;
    }
  }

  for (auto& slot : gDeviceSlots) {
    if (slot.handle != nullptr) continue;

    i2c_device_config_t deviceConfig = {};
    deviceConfig.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    deviceConfig.device_address = address;
    deviceConfig.scl_speed_hz = CARDPUTER_I2C_SPEED_HZ;

    const esp_err_t err = i2c_master_bus_add_device(gBusHandle, &deviceConfig, &slot.handle);
    if (err != ESP_OK) return err;

    slot.address = address;
    *out = slot.handle;
    return ESP_OK;
  }

  return ESP_ERR_NO_MEM;
}

}  // namespace

esp_err_t ensureI2cBus() {
  if (!ensureI2cMutex()) return ESP_ERR_NO_MEM;
  if (gBusHandle != nullptr) return ESP_OK;

  esp_err_t err = i2c_master_get_bus_handle(CARDPUTER_I2C_PORT, &gBusHandle);
  if (err == ESP_OK) return ESP_OK;

  err = createI2cBus();
  if (err == ESP_ERR_INVALID_STATE) {
    return i2c_master_get_bus_handle(CARDPUTER_I2C_PORT, &gBusHandle);
  }
  return err;
}

esp_err_t i2cProbe(uint16_t address, int timeoutMs) {
  esp_err_t err = ensureI2cBus();
  if (err != ESP_OK) return err;

  xSemaphoreTake(gI2cMutex, portMAX_DELAY);
  err = i2c_master_probe(gBusHandle, address, timeoutMs);
  xSemaphoreGive(gI2cMutex);
  return err;
}

esp_err_t i2cWrite(uint16_t address, const uint8_t* data, size_t length, int timeoutMs) {
  if (data == nullptr && length > 0) return ESP_ERR_INVALID_ARG;

  esp_err_t err = ensureI2cBus();
  if (err != ESP_OK) return err;

  xSemaphoreTake(gI2cMutex, portMAX_DELAY);
  i2c_master_dev_handle_t device = nullptr;
  err = deviceHandleForAddress(address, &device);
  if (err == ESP_OK) {
    err = i2c_master_transmit(device, data, length, timeoutMs);
  }
  xSemaphoreGive(gI2cMutex);
  return err;
}

esp_err_t i2cWriteRead(uint16_t address,
                       const uint8_t* writeData,
                       size_t writeLength,
                       uint8_t* readData,
                       size_t readLength,
                       int timeoutMs) {
  if ((writeData == nullptr && writeLength > 0) || (readData == nullptr && readLength > 0)) {
    return ESP_ERR_INVALID_ARG;
  }

  esp_err_t err = ensureI2cBus();
  if (err != ESP_OK) return err;

  xSemaphoreTake(gI2cMutex, portMAX_DELAY);
  i2c_master_dev_handle_t device = nullptr;
  err = deviceHandleForAddress(address, &device);
  if (err == ESP_OK) {
    err = i2c_master_transmit_receive(device, writeData, writeLength, readData, readLength, timeoutMs);
  }
  xSemaphoreGive(gI2cMutex);
  return err;
}
