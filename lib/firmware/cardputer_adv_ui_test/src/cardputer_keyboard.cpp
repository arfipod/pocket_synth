#include "cardputer_keyboard.h"

#include "freertos/FreeRTOS.h"
#include "esp_err.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include <stdio.h>
#include <string.h>

static const char* TAG = "cardputer_keyboard";

static constexpr i2c_port_t I2C_PORT = I2C_NUM_0;
static constexpr gpio_num_t I2C_SDA = GPIO_NUM_8;
static constexpr gpio_num_t I2C_SCL = GPIO_NUM_9;
static constexpr gpio_num_t TCA8418_INT = GPIO_NUM_11;
static constexpr uint8_t TCA8418_ADDR = 0x34;

static constexpr uint8_t REG_CFG = 0x01;
static constexpr uint8_t REG_INT_STAT = 0x02;
static constexpr uint8_t REG_KEY_LCK_EC = 0x03;
static constexpr uint8_t REG_KEY_EVENT_A = 0x04;
static constexpr uint8_t REG_GPIO_INT_STAT_1 = 0x11;
static constexpr uint8_t REG_GPIO_INT_STAT_2 = 0x12;
static constexpr uint8_t REG_GPIO_INT_STAT_3 = 0x13;
static constexpr uint8_t REG_GPIO_INT_EN_1 = 0x1A;
static constexpr uint8_t REG_GPIO_INT_EN_2 = 0x1B;
static constexpr uint8_t REG_GPIO_INT_EN_3 = 0x1C;
static constexpr uint8_t REG_KP_GPIO_1 = 0x1D;
static constexpr uint8_t REG_KP_GPIO_2 = 0x1E;
static constexpr uint8_t REG_KP_GPIO_3 = 0x1F;
static constexpr uint8_t REG_GPI_EM_1 = 0x20;
static constexpr uint8_t REG_GPI_EM_2 = 0x21;
static constexpr uint8_t REG_GPI_EM_3 = 0x22;
static constexpr uint8_t REG_GPIO_DIR_1 = 0x23;
static constexpr uint8_t REG_GPIO_DIR_2 = 0x24;
static constexpr uint8_t REG_GPIO_DIR_3 = 0x25;
static constexpr uint8_t REG_GPIO_INT_LVL_1 = 0x26;
static constexpr uint8_t REG_GPIO_INT_LVL_2 = 0x27;
static constexpr uint8_t REG_GPIO_INT_LVL_3 = 0x28;
static constexpr uint8_t REG_DEBOUNCE_DIS_1 = 0x29;
static constexpr uint8_t REG_DEBOUNCE_DIS_2 = 0x2A;
static constexpr uint8_t REG_DEBOUNCE_DIS_3 = 0x2B;

static constexpr uint8_t CFG_KE_IEN = 0x01;
static constexpr uint8_t CFG_GPI_IEN = 0x02;
static constexpr uint8_t INT_STAT_KEY = 0x01;
static constexpr uint8_t INT_STAT_GPIO = 0x02;
static constexpr uint8_t KEY_LEFT_CTRL = 0x80;
static constexpr uint8_t KEY_LEFT_SHIFT = 0x81;
static constexpr uint8_t KEY_LEFT_ALT = 0x82;
static constexpr uint8_t KEY_FN = 0xFF;
static constexpr uint8_t KEY_OPT = 0x00;
static constexpr uint8_t KEY_BACKSPACE = 0x2A;
static constexpr uint8_t KEY_TAB = 0x2B;
static constexpr uint8_t KEY_ENTER = 0x28;

struct KeyMapEntry {
  uint8_t normal;
  uint8_t shifted;
};

static constexpr KeyMapEntry KEY_MAP[4][14] = {
    {{'`', '~'}, {'1', '!'}, {'2', '@'}, {'3', '#'}, {'4', '$'}, {'5', '%'}, {'6', '^'},
     {'7', '&'}, {'8', '*'}, {'9', '('}, {'0', ')'}, {'-', '_'}, {'=', '+'}, {KEY_BACKSPACE, KEY_BACKSPACE}},
    {{KEY_TAB, KEY_TAB}, {'q', 'Q'}, {'w', 'W'}, {'e', 'E'}, {'r', 'R'}, {'t', 'T'}, {'y', 'Y'},
     {'u', 'U'}, {'i', 'I'}, {'o', 'O'}, {'p', 'P'}, {'[', '{'}, {']', '}'}, {'\\', '|'}},
    {{KEY_FN, KEY_FN}, {KEY_LEFT_SHIFT, KEY_LEFT_SHIFT}, {'a', 'A'}, {'s', 'S'}, {'d', 'D'}, {'f', 'F'}, {'g', 'G'},
     {'h', 'H'}, {'j', 'J'}, {'k', 'K'}, {'l', 'L'}, {';', ':'}, {'\'', '"'}, {KEY_ENTER, KEY_ENTER}},
    {{KEY_LEFT_CTRL, KEY_LEFT_CTRL}, {KEY_OPT, KEY_OPT}, {KEY_LEFT_ALT, KEY_LEFT_ALT}, {'z', 'Z'}, {'x', 'X'}, {'c', 'C'}, {'v', 'V'},
     {'b', 'B'}, {'n', 'N'}, {'m', 'M'}, {',', '<'}, {'.', '>'}, {'/', '?'}, {' ', ' '}},
};

bool CardputerKeyboard::begin() {
  if (initialized_) return true;
  setDiagnostic("creating i2c bus");

  i2c_config_t busConfig = {};
  busConfig.mode = I2C_MODE_MASTER;
  busConfig.sda_io_num = I2C_SDA;
  busConfig.scl_io_num = I2C_SCL;
  busConfig.sda_pullup_en = GPIO_PULLUP_ENABLE;
  busConfig.scl_pullup_en = GPIO_PULLUP_ENABLE;
  busConfig.master.clk_speed = 100000;
  busConfig.clk_flags = 0;

  esp_err_t err = i2c_param_config(I2C_PORT, &busConfig);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "i2c_param_config failed: %s", esp_err_to_name(err));
    snprintf(diagnostic_, sizeof(diagnostic_), "param failed %s", esp_err_to_name(err));
    return false;
  }

  err = i2c_driver_install(I2C_PORT, I2C_MODE_MASTER, 0, 0, 0);
  if (err == ESP_ERR_INVALID_STATE) {
    busInstalled_ = true;
  } else if (err != ESP_OK) {
    ESP_LOGE(TAG, "i2c_driver_install failed: %s", esp_err_to_name(err));
    snprintf(diagnostic_, sizeof(diagnostic_), "install failed %s", esp_err_to_name(err));
    return false;
  } else {
    busInstalled_ = true;
  }

  setDiagnostic("reading tca8418");
  uint8_t cfg = 0;
  if (!readRegister(REG_CFG, &cfg)) {
    ESP_LOGE(TAG, "TCA8418 not found at 0x%02x", TCA8418_ADDR);
    scanBus(diagnostic_, sizeof(diagnostic_));
    return false;
  }

  gpio_config_t intConfig = {};
  intConfig.pin_bit_mask = 1ULL << TCA8418_INT;
  intConfig.mode = GPIO_MODE_INPUT;
  intConfig.pull_up_en = GPIO_PULLUP_ENABLE;
  intConfig.pull_down_en = GPIO_PULLDOWN_DISABLE;
  intConfig.intr_type = GPIO_INTR_DISABLE;
  gpio_config(&intConfig);

  // Same TCA8418 setup path used by M5Cardputer/Adafruit_TCA8418:
  // default GPIO inputs, GPI event/interrupt mode configured, then 7x8 keypad matrix.
  setDiagnostic("configuring tca8418");
  if (!writeRegister(REG_GPIO_DIR_1, 0x00) ||
      !writeRegister(REG_GPIO_DIR_2, 0x00) ||
      !writeRegister(REG_GPIO_DIR_3, 0x00) ||
      !writeRegister(REG_GPI_EM_1, 0xFF) ||
      !writeRegister(REG_GPI_EM_2, 0xFF) ||
      !writeRegister(REG_GPI_EM_3, 0xFF) ||
      !writeRegister(REG_GPIO_INT_LVL_1, 0x00) ||
      !writeRegister(REG_GPIO_INT_LVL_2, 0x00) ||
      !writeRegister(REG_GPIO_INT_LVL_3, 0x00) ||
      !writeRegister(REG_GPIO_INT_EN_1, 0xFF) ||
      !writeRegister(REG_GPIO_INT_EN_2, 0xFF) ||
      !writeRegister(REG_GPIO_INT_EN_3, 0xFF) ||
      !writeRegister(REG_DEBOUNCE_DIS_1, 0x00) ||
      !writeRegister(REG_DEBOUNCE_DIS_2, 0x00) ||
      !writeRegister(REG_DEBOUNCE_DIS_3, 0x00) ||
      !writeRegister(REG_KP_GPIO_1, 0x7F) ||
      !writeRegister(REG_KP_GPIO_2, 0xFF) ||
      !writeRegister(REG_KP_GPIO_3, 0x00) ||
      !writeRegister(REG_CFG, cfg | CFG_GPI_IEN | CFG_KE_IEN)) {
    ESP_LOGE(TAG, "TCA8418 matrix setup failed");
    return false;
  }

  flush();

  uint8_t intStat = 0;
  uint8_t eventCount = 0;
  uint8_t kp1 = 0;
  uint8_t kp2 = 0;
  uint8_t kp3 = 0;
  readRegister(REG_CFG, &cfg);
  readRegister(REG_INT_STAT, &intStat);
  readRegister(REG_KEY_LCK_EC, &eventCount);
  readRegister(REG_KP_GPIO_1, &kp1);
  readRegister(REG_KP_GPIO_2, &kp2);
  readRegister(REG_KP_GPIO_3, &kp3);

  initialized_ = true;
  ESP_LOGI(TAG,
           "keyboard initialized addr=0x%02x sda=%d scl=%d int=%d int_level=%d cfg=0x%02x int_stat=0x%02x events=%u kp=[0x%02x 0x%02x 0x%02x]",
           TCA8418_ADDR,
           I2C_SDA,
           I2C_SCL,
           TCA8418_INT,
           gpio_get_level(TCA8418_INT),
           cfg,
           intStat,
           eventCount & 0x0F,
           kp1,
           kp2,
           kp3);
  setDiagnostic("ready");
  return true;
}

bool CardputerKeyboard::readEvent(CardputerKeyEvent* event) {
  if (!initialized_ || event == nullptr) return false;

  const uint8_t count = available();
  for (uint8_t i = 0; i < count; ++i) {
    CardputerKeyEvent decoded;
    const uint8_t rawEvent = getEvent();
    writeRegister(REG_INT_STAT, INT_STAT_KEY);
    if (!decodeEvent(rawEvent, &decoded)) continue;
    *event = decoded;
    return true;
  }

  return false;
}

CardputerKey CardputerKeyboard::readKey() {
  if (!initialized_) return CardputerKey::None;

  CardputerKeyEvent event;
  while (readEvent(&event)) {
    if (event.pressed && event.key != CardputerKey::Fn && event.key != CardputerKey::Shift &&
        event.key != CardputerKey::Ctrl && event.key != CardputerKey::Alt && event.key != CardputerKey::Opt) {
      return event.key;
    }
  }
  return CardputerKey::None;
}

const char* CardputerKeyboard::keyName(const CardputerKeyEvent& event) const {
  return keyName(event.key, event.character);
}

const char* CardputerKeyboard::keyName(CardputerKey key, char character) {
  static char characterName[8];

  switch (key) {
    case CardputerKey::Character:
      characterName[0] = character ? character : '?';
      characterName[1] = '\0';
      return characterName;
    case CardputerKey::Left:
      return "Left";
    case CardputerKey::Right:
      return "Right";
    case CardputerKey::Up:
      return "Up";
    case CardputerKey::Down:
      return "Down";
    case CardputerKey::DeleteForward:
      return "Delete";
    case CardputerKey::F1:
      return "F1";
    case CardputerKey::F2:
      return "F2";
    case CardputerKey::F3:
      return "F3";
    case CardputerKey::F4:
      return "F4";
    case CardputerKey::F5:
      return "F5";
    case CardputerKey::F6:
      return "F6";
    case CardputerKey::F7:
      return "F7";
    case CardputerKey::F8:
      return "F8";
    case CardputerKey::F9:
      return "F9";
    case CardputerKey::F10:
      return "F10";
    case CardputerKey::Enter:
      return "Enter";
    case CardputerKey::Esc:
      return "Esc";
    case CardputerKey::Backspace:
      return "Backspace";
    case CardputerKey::Tab:
      return "Tab";
    case CardputerKey::Space:
      return "Space";
    case CardputerKey::Fn:
      return "Fn";
    case CardputerKey::Shift:
      return "Shift";
    case CardputerKey::Ctrl:
      return "Ctrl";
    case CardputerKey::Alt:
      return "Alt";
    case CardputerKey::Opt:
      return "Opt";
    default:
      return "None";
  }
}

bool CardputerKeyboard::writeRegister(uint8_t reg, uint8_t value) {
  const uint8_t data[2] = {reg, value};
  return i2c_master_write_to_device(I2C_PORT, TCA8418_ADDR, data, sizeof(data), pdMS_TO_TICKS(50)) == ESP_OK;
}

bool CardputerKeyboard::readRegister(uint8_t reg, uint8_t* value) {
  return i2c_master_write_read_device(I2C_PORT, TCA8418_ADDR, &reg, 1, value, 1, pdMS_TO_TICKS(50)) == ESP_OK;
}

void CardputerKeyboard::setDiagnostic(const char* message) {
  snprintf(diagnostic_, sizeof(diagnostic_), "%s", message ? message : "");
}

void CardputerKeyboard::scanBus(char* out, size_t outSize) {
  bool found = false;
  size_t used = 0;
  if (out != nullptr && outSize > 0) {
    used = snprintf(out, outSize, "i2c:");
  }
  for (uint8_t addr = 0x08; addr < 0x78; ++addr) {
    uint8_t dummy = 0;
    if (i2c_master_write_read_device(I2C_PORT, addr, &dummy, 1, &dummy, 1, pdMS_TO_TICKS(20)) == ESP_OK) {
      ESP_LOGI(TAG, "i2c device found at 0x%02x", addr);
      if (out != nullptr && used < outSize) {
        used += snprintf(out + used, outSize - used, " %02X", addr);
      }
      found = true;
    }
  }
  if (!found) {
    ESP_LOGW(TAG, "i2c scan found no devices on GPIO%d/GPIO%d", I2C_SDA, I2C_SCL);
    if (out != nullptr && outSize > 0) {
      snprintf(out, outSize, "i2c: none");
    }
  }
}

uint8_t CardputerKeyboard::available() {
  uint8_t value = 0;
  if (!readRegister(REG_KEY_LCK_EC, &value)) return 0;
  return value & 0x0F;
}

uint8_t CardputerKeyboard::getEvent() {
  uint8_t value = 0;
  readRegister(REG_KEY_EVENT_A, &value);
  return value;
}

void CardputerKeyboard::flush() {
  while (getEvent() != 0) {
  }
  uint8_t unused = 0;
  readRegister(REG_GPIO_INT_STAT_1, &unused);
  readRegister(REG_GPIO_INT_STAT_2, &unused);
  readRegister(REG_GPIO_INT_STAT_3, &unused);
  writeRegister(REG_INT_STAT, INT_STAT_KEY | INT_STAT_GPIO);
}

bool CardputerKeyboard::decodeEvent(uint8_t rawEvent, CardputerKeyEvent* event) {
  if (rawEvent == 0 || event == nullptr) return false;

  bool pressed = false;
  KeyPosition position = {};
  if (!eventToPosition(rawEvent, &pressed, &position)) return false;

  updatePressedState(position, pressed);

  event->pressed = pressed;
  event->row = position.row;
  event->col = position.col;
  event->raw = rawEvent;
  applyModifiers(event);

  event->key = mapKey(position, &event->character);
  if (event->pressed && event->fn && event->key != CardputerKey::Fn) {
    const CardputerKey fnKey = mapFnKey(position);
    if (fnKey != CardputerKey::None) event->key = fnKey;
  }

  ESP_LOGI(TAG,
           "key raw=0x%02x %s row=%u col=%u key=%s char=0x%02x fn=%d shift=%d ctrl=%d alt=%d opt=%d",
           rawEvent,
           pressed ? "down" : "up",
           position.row,
           position.col,
           keyName(*event),
           static_cast<unsigned char>(event->character),
           event->fn ? 1 : 0,
           event->shift ? 1 : 0,
           event->ctrl ? 1 : 0,
           event->alt ? 1 : 0,
           event->opt ? 1 : 0);

  return true;
}

bool CardputerKeyboard::eventToPosition(uint8_t rawEvent, bool* pressed, KeyPosition* position) const {
  if (pressed == nullptr || position == nullptr) return false;

  uint8_t raw = rawEvent & 0x7F;
  if (raw == 0) return false;
  raw -= 1;

  const uint8_t electricalRow = raw / 10;
  const uint8_t electricalCol = raw % 10;
  const uint8_t mappedRow = (electricalCol + 4) % 4;
  const uint8_t mappedCol = electricalRow * 2 + (electricalCol > 3 ? 1 : 0);

  if (mappedRow >= 4 || mappedCol >= 14) return false;

  *pressed = (rawEvent & 0x80) != 0;
  position->row = mappedRow;
  position->col = mappedCol;
  return true;
}

void CardputerKeyboard::updatePressedState(KeyPosition position, bool pressed) {
  if (position.row >= 4 || position.col >= 14) return;
  pressed_[position.row][position.col] = pressed;
}

void CardputerKeyboard::applyModifiers(CardputerKeyEvent* event) const {
  if (event == nullptr) return;
  event->fn = isPositionPressed(2, 0);
  event->shift = isPositionPressed(2, 1);
  event->ctrl = isPositionPressed(3, 0);
  event->opt = isPositionPressed(3, 1);
  event->alt = isPositionPressed(3, 2);
}

CardputerKey CardputerKeyboard::mapKey(KeyPosition position, char* character) const {
  if (character != nullptr) *character = '\0';

  const bool shifted = isPositionPressed(2, 1) || isPositionPressed(3, 0);
  const uint8_t value = keyValue(position, shifted);
  switch (value) {
    case KEY_FN:
      return CardputerKey::Fn;
    case KEY_LEFT_SHIFT:
      return CardputerKey::Shift;
    case KEY_LEFT_CTRL:
      return CardputerKey::Ctrl;
    case KEY_LEFT_ALT:
      return CardputerKey::Alt;
    case KEY_OPT:
      return CardputerKey::Opt;
    case KEY_TAB:
      return CardputerKey::Tab;
    case KEY_BACKSPACE:
      return CardputerKey::Backspace;
    case KEY_ENTER:
      return CardputerKey::Enter;
    case ' ':
      if (character != nullptr) *character = ' ';
      return CardputerKey::Space;
    default:
      if (value >= 0x20 && value <= 0x7E) {
        if (character != nullptr) *character = static_cast<char>(value);
        return CardputerKey::Character;
      }
      return CardputerKey::None;
  }
}

CardputerKey CardputerKeyboard::mapFnKey(KeyPosition position) const {
  if (position.row == 0 && position.col == 0) return CardputerKey::Esc;
  if (position.row == 0 && position.col >= 1 && position.col <= 9) {
    return static_cast<CardputerKey>(static_cast<int>(CardputerKey::F1) + position.col - 1);
  }
  if (position.row == 0 && position.col == 10) return CardputerKey::F10;
  if (position.row == 0 && position.col == 13) return CardputerKey::DeleteForward;
  if (position.row == 2 && position.col == 11) return CardputerKey::Up;
  if (position.row == 3 && position.col == 10) return CardputerKey::Left;
  if (position.row == 3 && position.col == 11) return CardputerKey::Down;
  if (position.row == 3 && position.col == 12) return CardputerKey::Right;
  return CardputerKey::None;
}

uint8_t CardputerKeyboard::keyValue(KeyPosition position, bool shifted) const {
  if (position.row >= 4 || position.col >= 14) return 0;
  return shifted ? KEY_MAP[position.row][position.col].shifted : KEY_MAP[position.row][position.col].normal;
}

bool CardputerKeyboard::isPositionPressed(uint8_t row, uint8_t col) const {
  if (row >= 4 || col >= 14) return false;
  return pressed_[row][col];
}

bool CardputerKeyboard::isPressed(CardputerKey key) const {
  KeyPosition position = {};
  if (!findKeyPosition(key, '\0', &position)) return false;
  return isPositionPressed(position.row, position.col);
}

bool CardputerKeyboard::isCharacterPressed(char character) const {
  KeyPosition position = {};
  if (!findKeyPosition(CardputerKey::Character, character, &position)) return false;
  return isPositionPressed(position.row, position.col);
}

uint8_t CardputerKeyboard::pressedCount() const {
  uint8_t count = 0;
  for (uint8_t row = 0; row < 4; ++row) {
    for (uint8_t col = 0; col < 14; ++col) {
      if (pressed_[row][col]) ++count;
    }
  }
  return count;
}

bool CardputerKeyboard::findKeyPosition(CardputerKey key, char character, KeyPosition* position) const {
  if (position == nullptr) return false;

  if (key == CardputerKey::Left) {
    *position = {3, 10};
    return true;
  }
  if (key == CardputerKey::Down) {
    *position = {3, 11};
    return true;
  }
  if (key == CardputerKey::Right) {
    *position = {3, 12};
    return true;
  }
  if (key == CardputerKey::Up) {
    *position = {2, 11};
    return true;
  }
  if (key == CardputerKey::Esc) {
    *position = {0, 0};
    return true;
  }
  if (key == CardputerKey::DeleteForward || key == CardputerKey::Backspace) {
    *position = {0, 13};
    return true;
  }
  if (key >= CardputerKey::F1 && key <= CardputerKey::F10) {
    const int index = static_cast<int>(key) - static_cast<int>(CardputerKey::F1);
    *position = {0, static_cast<uint8_t>(index == 9 ? 10 : index + 1)};
    return true;
  }

  for (uint8_t row = 0; row < 4; ++row) {
    for (uint8_t col = 0; col < 14; ++col) {
      const KeyPosition candidate = {row, col};
      char mappedCharacter = '\0';
      const CardputerKey mapped = mapKey(candidate, &mappedCharacter);
      if (mapped == key && (key != CardputerKey::Character || mappedCharacter == character)) {
        *position = candidate;
        return true;
      }
    }
  }
  return false;
}
