#pragma once

#include <stdint.h>

enum class CardputerKey {
  None,
  Character,
  Left,
  Right,
  Up,
  Down,
  DeleteForward,
  F1,
  F2,
  F3,
  F4,
  F5,
  F6,
  F7,
  F8,
  F9,
  F10,
  Enter,
  Esc,
  Backspace,
  Tab,
  Space,
  Fn,
  Shift,
  Ctrl,
  Alt,
  Opt,
};

struct CardputerKeyEvent {
  CardputerKey key = CardputerKey::None;
  char character = '\0';
  bool pressed = false;
  bool fn = false;
  bool shift = false;
  bool ctrl = false;
  bool alt = false;
  bool opt = false;
  uint8_t row = 0;
  uint8_t col = 0;
  uint8_t raw = 0;
};

class CardputerKeyboard {
 public:
  bool begin();
  bool readEvent(CardputerKeyEvent* event);
  CardputerKey readKey();
  bool isPressed(CardputerKey key) const;
  bool isCharacterPressed(char character) const;
  uint8_t pressedCount() const;
  bool isReady() const { return initialized_; }
  const char* diagnostic() const { return diagnostic_; }
  const char* keyName(const CardputerKeyEvent& event) const;
  static const char* keyName(CardputerKey key, char character = '\0');

 private:
  struct KeyPosition {
    uint8_t row;
    uint8_t col;
  };

  bool writeRegister(uint8_t reg, uint8_t value);
  bool readRegister(uint8_t reg, uint8_t* value);
  void setDiagnostic(const char* message);
  void scanBus(char* out, size_t outSize);
  uint8_t available();
  uint8_t getEvent();
  void flush();
  bool decodeEvent(uint8_t rawEvent, CardputerKeyEvent* event);
  bool eventToPosition(uint8_t rawEvent, bool* pressed, KeyPosition* position) const;
  void updatePressedState(KeyPosition position, bool pressed);
  void applyModifiers(CardputerKeyEvent* event) const;
  CardputerKey mapKey(KeyPosition position, char* character) const;
  CardputerKey mapFnKey(KeyPosition position) const;
  uint8_t keyValue(KeyPosition position, bool shifted) const;
  bool isPositionPressed(uint8_t row, uint8_t col) const;
  bool findKeyPosition(CardputerKey key, char character, KeyPosition* position) const;

  bool initialized_ = false;
  char diagnostic_[48] = "not started";
  bool pressed_[4][14] = {};
  bool busInstalled_ = false;
};
