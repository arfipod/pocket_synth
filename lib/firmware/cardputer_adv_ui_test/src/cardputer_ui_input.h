#pragma once

#include "cardputer_keyboard.h"
#include "generated/cardputer_ui.h"

struct CardputerUiInputEvent {
  CardputerUiEvent event = CARDPUTER_UI_EVENT_PRESS;
  bool valid = false;
};

class CardputerUiInputMapper {
 public:
  static bool mapKeyEvent(const CardputerKeyEvent& keyEvent, CardputerUiInputEvent* first, CardputerUiInputEvent* second = nullptr);
};
