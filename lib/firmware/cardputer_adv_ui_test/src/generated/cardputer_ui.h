#pragma once

#include "../cardputer_display.h"

enum CardputerScreenId {
  CARDPUTER_SCREEN_POCKETSYNTH_MAIN_SINGLE_VOICE_COMPACT_8_POLY = 0
};
static constexpr CardputerScreenId CARDPUTER_UI_START_SCREEN = CARDPUTER_SCREEN_POCKETSYNTH_MAIN_SINGLE_VOICE_COMPACT_8_POLY;

enum CardputerUiEvent {
  CARDPUTER_UI_EVENT_PRESS,
  CARDPUTER_UI_EVENT_LONG_PRESS,
  CARDPUTER_UI_EVENT_KEY_ENTER,
  CARDPUTER_UI_EVENT_KEY_BACK,
  CARDPUTER_UI_EVENT_SOFTKEY_LEFT,
  CARDPUTER_UI_EVENT_SOFTKEY_RIGHT
};

void cardputer_ui_init(CardputerDisplay* display);
void cardputer_ui_draw(CardputerScreenId screen);
CardputerScreenId cardputer_ui_handle_event(CardputerScreenId current, CardputerUiEvent event);
CardputerScreenId cardputer_ui_handle_element_event(CardputerScreenId current, const char* elementId, CardputerUiEvent event);
