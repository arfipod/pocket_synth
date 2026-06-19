#pragma once

#include "driver/gpio.h"

// I2C shared bus
inline constexpr gpio_num_t PIN_I2C_SDA = GPIO_NUM_8;
inline constexpr gpio_num_t PIN_I2C_SCL = GPIO_NUM_9;

// LCD ST7789
inline constexpr gpio_num_t PIN_LCD_RST = GPIO_NUM_33;
inline constexpr gpio_num_t PIN_LCD_RDC = GPIO_NUM_34;
inline constexpr gpio_num_t PIN_LCD_MOSI = GPIO_NUM_35;
inline constexpr gpio_num_t PIN_LCD_SCLK = GPIO_NUM_36;
inline constexpr gpio_num_t PIN_LCD_CS = GPIO_NUM_37;
inline constexpr gpio_num_t PIN_LCD_BL = GPIO_NUM_38;

// microSD SPI
inline constexpr gpio_num_t PIN_SD_CS = GPIO_NUM_12;
inline constexpr gpio_num_t PIN_SD_MOSI = GPIO_NUM_14;
inline constexpr gpio_num_t PIN_SD_CLK = GPIO_NUM_40;
inline constexpr gpio_num_t PIN_SD_MISO = GPIO_NUM_39;

// I2S / ES8311
inline constexpr gpio_num_t PIN_I2S_BCLK = GPIO_NUM_41;
inline constexpr gpio_num_t PIN_I2S_DOUT = GPIO_NUM_42;
inline constexpr gpio_num_t PIN_I2S_LRCK = GPIO_NUM_43;
inline constexpr gpio_num_t PIN_I2S_DIN = GPIO_NUM_46;
inline constexpr gpio_num_t PIN_I2S_MCLK = GPIO_NUM_NC;

// Other internal/peripheral pins
inline constexpr gpio_num_t PIN_IR_TX = GPIO_NUM_44;
inline constexpr gpio_num_t PIN_RGB_LED = GPIO_NUM_21;
