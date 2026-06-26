#include "note_map.h"

namespace pocketsynth {
namespace {

constexpr KeyNote KEY_NOTES[] = {
    {'z', 0, 60},  {'s', 1, 61},  {'x', 2, 62},  {'d', 3, 63},  {'c', 4, 64},
    {'v', 5, 65},  {'g', 6, 66},  {'b', 7, 67},  {'h', 8, 68},  {'n', 9, 69},
    {'j', 10, 70}, {'m', 11, 71}, {'q', 12, 72}, {'2', 13, 73}, {'w', 14, 74},
    {'3', 15, 75}, {'e', 16, 76}, {'r', 17, 77}, {'5', 18, 78}, {'t', 19, 79},
    {'6', 20, 80}, {'y', 21, 81}, {'7', 22, 82}, {'u', 23, 83}, {'i', 24, 84},
};

char normalizeKeyChar(char ch) {
  if (ch >= 'A' && ch <= 'Z') return static_cast<char>(ch - 'A' + 'a');
  return ch;
}

}  // namespace

const KeyNote* findNoteByKey(char key) {
  const char normalized = normalizeKeyChar(key);
  for (const auto& entry : KEY_NOTES) {
    if (entry.key == normalized) return &entry;
  }
  return nullptr;
}

const KeyNote* findNoteByMidi(uint8_t midi) {
  for (const auto& entry : KEY_NOTES) {
    if (entry.midi == midi) return &entry;
  }
  return nullptr;
}

}  // namespace pocketsynth
