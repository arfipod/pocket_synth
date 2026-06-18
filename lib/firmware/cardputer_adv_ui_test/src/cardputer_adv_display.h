#pragma once

#include <M5GFX.h>
#include <lgfx/v1/panel/Panel_ST7789.hpp>
#include <lgfx/v1/platforms/esp32/Bus_SPI.hpp>
#include <lgfx/v1/platforms/esp32/Light_PWM.hpp>

class CardputerAdvDisplay : public lgfx::LGFX_Device {
  lgfx::Panel_ST7789 panel_;
  lgfx::Bus_SPI bus_;
  lgfx::Light_PWM backlight_;

public:
  CardputerAdvDisplay() {
    {
      lgfx::Bus_SPI::config_t cfg = bus_.config();
      cfg.spi_host = SPI3_HOST;
      cfg.spi_mode = 0;
      cfg.freq_write = 40000000;
      cfg.freq_read = 16000000;
      cfg.spi_3wire = true;
      cfg.use_lock = true;
      cfg.dma_channel = SPI_DMA_CH_AUTO;
      cfg.pin_sclk = 36;
      cfg.pin_mosi = 35;
      cfg.pin_miso = -1;
      cfg.pin_dc = 34;
      bus_.config(cfg);
      panel_.setBus(&bus_);
    }

    {
      lgfx::Panel_ST7789::config_t cfg = panel_.config();
      cfg.pin_cs = 37;
      cfg.pin_rst = 33;
      cfg.pin_busy = -1;
      cfg.memory_width = 240;
      cfg.memory_height = 320;
      cfg.panel_width = 135;
      cfg.panel_height = 240;
      cfg.offset_x = 52;
      cfg.offset_y = 40;
      cfg.offset_rotation = 0;
      cfg.dummy_read_pixel = 8;
      cfg.dummy_read_bits = 1;
      cfg.readable = true;
      cfg.invert = true;
      cfg.rgb_order = false;
      cfg.dlen_16bit = false;
      cfg.bus_shared = false;
      panel_.config(cfg);
    }

    {
      lgfx::Light_PWM::config_t cfg = backlight_.config();
      cfg.pin_bl = 38;
      cfg.invert = false;
      cfg.freq = 256;
      cfg.offset = 16;
      cfg.pwm_channel = 7;
      backlight_.config(cfg);
      panel_.setLight(&backlight_);
    }

    setPanel(&panel_);
  }
};
