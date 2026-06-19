# pocketsynth

`pocketsynth` is a real-time synthesizer for the M5Stack Cardputer ADV.
Iteration 1 validates the critical instrument chain: physical keyboard, note
on/off events, mathematical oscillators, up to 8 mixed notes, master volume,
audio buffer rendering, and I2S/speaker output.

The iteration 1 rule is simple: stable audio first, then interaction, then UI,
then musical analysis.

## Current State

- Main PlatformIO/ESP-IDF project at the repo root.
- Refactored firmware split across focused modules in `include/` and `src/`.
- Cardputer ADV UI test bench in `lib/firmware/cardputer_adv_ui_test`.
- Original design references stored in `docs/references/`.

## Documentation

- `docs/iteration-1.md`: full scope of the first iteration.
- `docs/architecture.md`: FreeRTOS architecture, state flow, and real-time rules.
- `docs/ui.md`: compact UI, controls, colors, and JSON reference.
- `docs/implementation-plan.md`: phases 1A-1G with acceptance criteria.
- `docs/decisions.md`: initial technical decisions.
- `AGENTS.md`: guide for code agents working in this repo.

## Build

Main firmware:

```powershell
pio run -e cardputer_adv
```

UI test bench:

```powershell
pio run -d lib/firmware/cardputer_adv_ui_test -e cardputer_adv
```

## Target Hardware

- M5Stack Cardputer ADV.
- 240 x 135 px ST7789V2 display in landscape orientation.
- I2S/speaker audio output.
- Integrated physical keyboard.
