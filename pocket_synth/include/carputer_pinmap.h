#pragma once
#include "driver/gpio.h"

// I2C shared bus
static constexpr gpio_num_t PIN_I2C_SDA = GPIO_NUM_8;
static constexpr gpio_num_t PIN_I2C_SCL = GPIO_NUM_9;

// LCD ST7789
static constexpr gpio_num_t PIN_LCD_RST = GPIO_NUM_33;
static constexpr gpio_num_t PIN_LCD_RDC = GPIO_NUM_34;
static constexpr gpio_num_t PIN_LCD_MOSI = GPIO_NUM_35;
static constexpr gpio_num_t PIN_LCD_SCLK = GPIO_NUM_36;
static constexpr gpio_num_t PIN_LCD_CS = GPIO_NUM_37;
static constexpr gpio_num_t PIN_LCD_BL = GPIO_NUM_38;

// microSD SPI
static constexpr gpio_num_t PIN_SD_CS = GPIO_NUM_12;
static constexpr gpio_num_t PIN_SD_MOSI = GPIO_NUM_14;
static constexpr gpio_num_t PIN_SD_CLK = GPIO_NUM_40;
static constexpr gpio_num_t PIN_SD_MISO = GPIO_NUM_39;

// I2S / ES8311
static constexpr gpio_num_t PIN_I2S_BCLK = GPIO_NUM_41;
static constexpr gpio_num_t PIN_I2S_DOUT = GPIO_NUM_42;
static constexpr gpio_num_t PIN_I2S_LRCK = GPIO_NUM_43;
static constexpr gpio_num_t PIN_I2S_DIN = GPIO_NUM_46;
static constexpr gpio_num_t PIN_I2S_MCLK = GPIO_NUM_NC;

// Other internal/peripheral pins
static constexpr gpio_num_t PIN_IR_TX = GPIO_NUM_44;
static constexpr gpio_num_t PIN_RGB_LED = GPIO_NUM_21;
