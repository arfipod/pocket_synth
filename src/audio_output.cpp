#include "audio_output.h"

#include "cardputer_pinmap.h"
#include "es8311_reg.h"
#include "synth_config.h"

#include "driver/i2c.h"
#include "driver/i2s_std.h"
#include "esp_check.h"
#include "soc/soc_caps.h"
#if defined(CONFIG_IDF_TARGET_ESP32S3)
#include "hal/i2s_ll.h"
#include "soc/i2s_struct.h"
#endif

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <math.h>

namespace pocketsynth {
namespace {

constexpr const char* TAG = "audio_output";
constexpr uint8_t ES8311_ADDRESS = 0x18;
i2s_chan_handle_t gI2sTx = nullptr;

bool probeI2cDevice(uint8_t address, uint8_t reg) {
  uint8_t value = 0;
  return i2c_master_write_read_device(I2C_NUM_0, address, &reg, 1, &value, 1, pdMS_TO_TICKS(50)) == ESP_OK;
}

esp_err_t writeCodecReg(uint8_t reg, uint8_t value) {
  const uint8_t data[2] = {reg, value};
  return i2c_master_write_to_device(I2C_NUM_0, ES8311_ADDRESS, data, sizeof(data), pdMS_TO_TICKS(100));
}

#if defined(CONFIG_IDF_TARGET_ESP32S3)
void calcI2sClockDiv(uint32_t* divA, uint32_t* divB, uint32_t* divN, uint32_t baseClock,
                     uint32_t targetFrequency) {
  if (baseClock <= (targetFrequency << 1)) {
    *divN = 2;
    *divA = 1;
    *divB = 0;
    return;
  }

  uint32_t saveN = 255;
  uint32_t saveA = 63;
  uint32_t saveB = 62;

  if (targetFrequency > 0) {
    float div = static_cast<float>(baseClock) / static_cast<float>(targetFrequency);
    const uint32_t n = static_cast<uint32_t>(div);
    if (n < 256) {
      div -= static_cast<float>(n);

      float checkBase = static_cast<float>(baseClock);
      while (static_cast<int32_t>(targetFrequency) >= 0) {
        targetFrequency <<= 1;
        checkBase *= 2.0f;
      }
      const float checkTarget = static_cast<float>(targetFrequency);

      uint32_t saveDiff = UINT32_MAX;
      if (n < 255) {
        saveA = 1;
        saveB = 0;
        saveN = n + 1;
        saveDiff = static_cast<uint32_t>(fabsf(checkTarget - (checkBase / static_cast<float>(saveN))));
      }

      for (uint32_t a = 1; a < 64; ++a) {
        const uint32_t b = static_cast<uint32_t>(roundf(static_cast<float>(a) * div));
        if (a <= b) continue;

        const uint32_t diff =
            static_cast<uint32_t>(fabsf(checkTarget - ((checkBase * static_cast<float>(a)) /
                                                       static_cast<float>((n * a) + b))));
        if (saveDiff <= diff) continue;

        saveDiff = diff;
        saveA = a;
        saveB = b;
        saveN = n;
        if (diff == 0) break;
      }
    }
  }

  *divN = saveN;
  *divA = saveA;
  *divB = saveB;
}

void applyCardputerI2sClock() {
  constexpr uint32_t PLL_D2_CLK = 120 * 1000 * 1000;
  constexpr uint32_t SAMPLE_BITS = 16;
  constexpr uint32_t BCLK_DIV = 32 / SAMPLE_BITS;

  uint32_t divA = 0;
  uint32_t divB = 0;
  uint32_t divN = 0;
  calcI2sClockDiv(&divA, &divB, &divN, PLL_D2_CLK, BCLK_DIV * SAMPLE_BITS * SAMPLE_RATE);

  i2s_dev_t* dev = &I2S1;
  i2s_ll_tx_clk_set_src(dev, I2S_CLK_SRC_PLL_240M);
  dev->tx_clkm_conf.clk_en = 1;
  dev->tx_clkm_conf.tx_clk_active = 1;

  dev->tx_conf.tx_mono = 1;
  dev->tx_conf.tx_chan_equal = 1;
  dev->tx_conf1.tx_bck_div_num = BCLK_DIV - 1;

  const uint32_t yn1 = divB > (divA >> 1);
  if (yn1) divB = divA - divB;

  uint32_t divY = 1;
  uint32_t divX = 0;
  if (divB != 0) {
    divX = (divA / divB) - 1;
    divY = divA % divB;
    if (divY == 0) {
      divY = 1;
      divB = 511;
    }
  }

  i2s_ll_tx_set_raw_clk_div(dev, divN, divX, divY, divB, yn1);
  dev->tx_conf.tx_update = 1;
  dev->tx_conf.tx_update = 0;
}
#else
void applyCardputerI2sClock() {}
#endif

}  // namespace

esp_err_t ensureI2cBus() {
  i2c_config_t busConfig = {};
  busConfig.mode = I2C_MODE_MASTER;
  busConfig.sda_io_num = PIN_I2C_SDA;
  busConfig.scl_io_num = PIN_I2C_SCL;
  busConfig.sda_pullup_en = GPIO_PULLUP_ENABLE;
  busConfig.scl_pullup_en = GPIO_PULLUP_ENABLE;
  busConfig.master.clk_speed = 100000;
  busConfig.clk_flags = 0;

  esp_err_t err = i2c_param_config(I2C_NUM_0, &busConfig);
  if (err != ESP_OK) return err;

  err = i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);
  if (err == ESP_ERR_INVALID_STATE) return ESP_OK;
  return err;
}

esp_err_t initializeI2sOutput() {
  i2s_chan_config_t channelConfig = {};
  channelConfig.id = I2S_NUM_1;
  channelConfig.role = I2S_ROLE_MASTER;
  channelConfig.dma_desc_num = 6;
  channelConfig.dma_frame_num = AUDIO_BUFFER_FRAMES;
  channelConfig.auto_clear = true;
  channelConfig.auto_clear_after_cb = true;
  channelConfig.auto_clear_before_cb = false;
  channelConfig.allow_pd = false;
  channelConfig.intr_priority = 0;

  esp_err_t err = i2s_new_channel(&channelConfig, &gI2sTx, nullptr);
  if (err != ESP_OK) return err;

  i2s_std_config_t stdConfig = {};
  stdConfig.clk_cfg.sample_rate_hz = SAMPLE_RATE;
  stdConfig.clk_cfg.clk_src = I2S_CLK_SRC_PLL_240M;
#if SOC_I2S_HW_VERSION_2
  stdConfig.clk_cfg.ext_clk_freq_hz = 0;
#endif
  stdConfig.clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_128;
  stdConfig.clk_cfg.bclk_div = 8;

  stdConfig.slot_cfg.data_bit_width = I2S_DATA_BIT_WIDTH_16BIT;
  stdConfig.slot_cfg.slot_bit_width = I2S_SLOT_BIT_WIDTH_16BIT;
  stdConfig.slot_cfg.slot_mode = I2S_SLOT_MODE_MONO;
  stdConfig.slot_cfg.slot_mask = I2S_STD_SLOT_BOTH;
  stdConfig.slot_cfg.ws_width = I2S_DATA_BIT_WIDTH_16BIT;
  stdConfig.slot_cfg.ws_pol = false;
  stdConfig.slot_cfg.bit_shift = true;
#if SOC_I2S_HW_VERSION_1
  stdConfig.slot_cfg.msb_right = true;
#else
  stdConfig.slot_cfg.left_align = true;
  stdConfig.slot_cfg.big_endian = false;
  stdConfig.slot_cfg.bit_order_lsb = false;
#endif

  stdConfig.gpio_cfg.mclk = I2S_GPIO_UNUSED;
  stdConfig.gpio_cfg.bclk = PIN_I2S_BCLK;
  stdConfig.gpio_cfg.ws = PIN_I2S_LRCK;
  stdConfig.gpio_cfg.dout = PIN_I2S_DOUT;
  stdConfig.gpio_cfg.din = PIN_I2S_DIN;
  stdConfig.gpio_cfg.invert_flags.mclk_inv = false;
  stdConfig.gpio_cfg.invert_flags.bclk_inv = false;
  stdConfig.gpio_cfg.invert_flags.ws_inv = false;

  err = i2s_channel_init_std_mode(gI2sTx, &stdConfig);
  if (err != ESP_OK) return err;

  applyCardputerI2sClock();
  return i2s_channel_enable(gI2sTx);
}

esp_err_t initializeCodecOutput() {
  for (int attempt = 0; attempt < 12; ++attempt) {
    if (probeI2cDevice(ES8311_ADDRESS, ES8311_CHD1_REGFD)) break;
    vTaskDelay(pdMS_TO_TICKS(50));
  }

  constexpr uint8_t CARDPUTER_ADV_DAC_INIT[][2] = {
      {ES8311_RESET_REG00, 0x80},
      {ES8311_CLK_MANAGER_REG01, 0xB5},
      {ES8311_CLK_MANAGER_REG02, 0x18},
      {ES8311_SYSTEM_REG0D, 0x01},
      {ES8311_SYSTEM_REG12, 0x00},
      {ES8311_SYSTEM_REG13, 0x10},
      {ES8311_DAC_REG32, 0xBF},
      {ES8311_DAC_REG37, 0x08},
  };

  for (const auto& regValue : CARDPUTER_ADV_DAC_INIT) {
    ESP_RETURN_ON_ERROR(writeCodecReg(regValue[0], regValue[1]), TAG, "ES8311 register init failed");
  }

  return ESP_OK;
}

void writeAudioFrames(const int32_t* buffer, size_t frames) {
  if (gI2sTx == nullptr || buffer == nullptr) {
    vTaskDelay(pdMS_TO_TICKS(10));
    return;
  }

  size_t bytesWritten = 0;
  i2s_channel_write(gI2sTx, buffer, frames * sizeof(int16_t), &bytesWritten, portMAX_DELAY);
}

}  // namespace pocketsynth
