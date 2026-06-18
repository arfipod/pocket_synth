#include "cardputer_ui_input.h"

bool CardputerUiInputMapper::mapKeyEvent(const CardputerKeyEvent& keyEvent,
                                         CardputerUiInputEvent* first,
                                         CardputerUiInputEvent* second) {
  if (first == nullptr) return false;
  first->valid = false;
  if (second != nullptr) second->valid = false;
  if (!keyEvent.pressed) return false;

  switch (keyEvent.key) {
    case CardputerKey::Left:
      first->event = CARDPUTER_UI_EVENT_SOFTKEY_LEFT;
      first->valid = true;
      return true;
    case CardputerKey::Right:
      first->event = CARDPUTER_UI_EVENT_SOFTKEY_RIGHT;
      first->valid = true;
      return true;
    case CardputerKey::Enter:
    case CardputerKey::Space:
      first->event = CARDPUTER_UI_EVENT_KEY_ENTER;
      first->valid = true;
      if (second != nullptr) {
        second->event = CARDPUTER_UI_EVENT_PRESS;
        second->valid = true;
      }
      return true;
    case CardputerKey::Esc:
    case CardputerKey::Backspace:
    case CardputerKey::DeleteForward:
      first->event = CARDPUTER_UI_EVENT_KEY_BACK;
      first->valid = true;
      return true;
    default:
      return false;
  }
}
