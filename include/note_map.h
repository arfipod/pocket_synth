#pragma once

#include <stdint.h>

namespace pocketsynth {

struct KeyNote {
  char key;
  uint8_t noteIndex;
  uint8_t midi;
};

const KeyNote* findNoteByKey(char key);
const KeyNote* findNoteByMidi(uint8_t midi);

}  // namespace pocketsynth
